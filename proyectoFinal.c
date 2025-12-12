/*Sistema Logístico: Árbol binario de búsqueda (BST) con balanceo AVL por fecha (AAAAMMDD)
   + colas FIFO de pedidos.
   Objetivo: despachar por lote más antiguo y mantener inventario. */

   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   
   #define MAX_NOMBRE 64
   
   /* Pedido en cola FIFO (destino, cantidad). */

   typedef struct Pedido {
       char destino[MAX_NOMBRE];
       int cantidad;
       struct Pedido *siguiente;
   } Pedido;
   
   /* Nodo AVL para un lote (fecha clave, stock, producto, cola de pedidos). */

   typedef struct NodoAVL {
       int fecha; // AAAAMMDD
       int stock;
       char producto[MAX_NOMBRE];
       Pedido *cabeza_pedidos;
       int altura;
       struct NodoAVL *izq;
       struct NodoAVL *der;
   } NodoAVL;

// Utilidades AVL//

   static int altura(NodoAVL *n) { return n ? n->altura : 0; }
   static int max(int a, int b) { return (a > b) ? a : b; }
   static void actualizar_altura(NodoAVL *n) {
       if (n) n->altura = 1 + max(altura(n->izq), altura(n->der));
   }
   static int factor_balance(NodoAVL *n) {
       return n ? (altura(n->izq) - altura(n->der)) : 0;
   }
   
   static NodoAVL *rotacion_derecha(NodoAVL *y) {
       NodoAVL *x = y->izq;
       NodoAVL *T2 = x->der;
       x->der = y;
       y->izq = T2;
       actualizar_altura(y);
       actualizar_altura(x);
       return x;
   }
   
   static NodoAVL *rotacion_izquierda(NodoAVL *x) {
       NodoAVL *y = x->der;
       NodoAVL *T2 = y->izq;
       y->izq = x;
       x->der = T2;
       actualizar_altura(x);
       actualizar_altura(y);
       return y;
   }
   
   static NodoAVL *balancear(NodoAVL *n) {
       if (!n) return n;
       actualizar_altura(n);
       int bf = factor_balance(n);
       if (bf > 1) {
           if (factor_balance(n->izq) < 0) n->izq = rotacion_izquierda(n->izq);
           return rotacion_derecha(n);
       } else if (bf < -1) {
           if (factor_balance(n->der) > 0) n->der = rotacion_derecha(n->der);
           return rotacion_izquierda(n);
       }
       return n;
   }

// Crear nodo AVL

   static NodoAVL *crear_nodo(int fecha, int stock, const char *producto) {
       NodoAVL *n = (NodoAVL *)malloc(sizeof(NodoAVL));
       if (!n) return NULL;
       n->fecha = fecha;
       n->stock = stock;
       strncpy(n->producto, producto, MAX_NOMBRE - 1);
       n->producto[MAX_NOMBRE - 1] = '\0';
       n->cabeza_pedidos = NULL;
       n->altura = 1;
       n->izq = n->der = NULL;
       return n;
   }
   
   // Buscar por fecha exacta
   static NodoAVL *buscar_por_fecha(NodoAVL *raiz, int fecha) {
       if (!raiz) return NULL;
       if (fecha < raiz->fecha) return buscar_por_fecha(raiz->izq, fecha);
       if (fecha > raiz->fecha) return buscar_por_fecha(raiz->der, fecha);
       return raiz;
   }
   
   // Insertar en AVL; devuelve nueva raíz y marca inserted=1 si insertó, 0 si duplicado
   static NodoAVL *insertar_avl(NodoAVL *nodo, int fecha, int stock, const char *producto, int *inserted) {
       if (!nodo) {
           *inserted = 1;
           return crear_nodo(fecha, stock, producto);
       }
       if (fecha < nodo->fecha) {
           nodo->izq = insertar_avl(nodo->izq, fecha, stock, producto, inserted);
       } else if (fecha > nodo->fecha) {
           nodo->der = insertar_avl(nodo->der, fecha, stock, producto, inserted);
       } else {
           // Clave duplicada
           *inserted = 0;
           return nodo;
       }
       return balancear(nodo);
   }
   
   // Mínimo por fecha (más antiguo)
   static NodoAVL *min_nodo(NodoAVL *nodo) {
       NodoAVL *actual = nodo;
       while (actual && actual->izq) actual = actual->izq;
       return actual;
   }
   
   // Liberar cola completa
   static void liberar_cola(Pedido *head) {
       while (head) {
           Pedido *tmp = head;
           head = head->siguiente;
           free(tmp);
       }
   }
   
   // Eliminar nodo por fecha en AVL; libera cola del nodo eliminado; devuelve nueva raíz
   static NodoAVL *eliminar_avl(NodoAVL *raiz, int fecha, int *eliminado) {
       if (!raiz) return NULL;
       if (fecha < raiz->fecha) {
           raiz->izq = eliminar_avl(raiz->izq, fecha, eliminado);
       } else if (fecha > raiz->fecha) {
           raiz->der = eliminar_avl(raiz->der, fecha, eliminado);
       } else {
           // Encontrado
           *eliminado = 1;
           if (!raiz->izq || !raiz->der) {
               NodoAVL *temp = raiz->izq ? raiz->izq : raiz->der;
               liberar_cola(raiz->cabeza_pedidos);
               free(raiz);
               return balancear(temp);
           } else {
               NodoAVL *s = min_nodo(raiz->der);
               // Transferir datos del sucesor. Evitar doble free de su cola.
               // Primero liberar la cola propia del nodo actual.
               liberar_cola(raiz->cabeza_pedidos);
               raiz->fecha = s->fecha;
               raiz->stock = s->stock;
               strncpy(raiz->producto, s->producto, MAX_NOMBRE - 1);
               raiz->producto[MAX_NOMBRE - 1] = '\0';
               raiz->cabeza_pedidos = s->cabeza_pedidos; // Tomar propiedad
               s->cabeza_pedidos = NULL; // Evita liberar pedidos al eliminar sucesor
               raiz->der = eliminar_avl(raiz->der, s->fecha, eliminado);
           }
       }
       return balancear(raiz);
   }
   
   static void liberar_avl(NodoAVL *raiz) {
       if (!raiz) return;
       liberar_avl(raiz->izq);
       liberar_avl(raiz->der);
       liberar_cola(raiz->cabeza_pedidos);
       free(raiz);
   }
   
   // FIFO: Encolar al final
   static int encolar_pedido(NodoAVL *nodo, const char *destino, int cantidad) {
       if (!nodo) return 0;
       Pedido *p = (Pedido *)malloc(sizeof(Pedido));
       if (!p) return 0;
       strncpy(p->destino, destino, MAX_NOMBRE - 1);
       p->destino[MAX_NOMBRE - 1] = '\0';
       p->cantidad = cantidad;
       p->siguiente = NULL;
       if (!nodo->cabeza_pedidos) {
           nodo->cabeza_pedidos = p;
       } else {
           Pedido *cur = nodo->cabeza_pedidos;
           while (cur->siguiente) cur = cur->siguiente;
           cur->siguiente = p;
       }
       return 1;
   }
   
   // FIFO: Cancelar pedido específico dentro de un nodo
   static int cancelar_pedido_en_nodo(NodoAVL *nodo, const char *destino, int cantidad) {
       if (!nodo) return 0;
       Pedido *cur = nodo->cabeza_pedidos;
       Pedido *prev = NULL;
       while (cur) {
           if (cur->cantidad == cantidad && strcmp(cur->destino, destino) == 0) {
               if (prev) prev->siguiente = cur->siguiente; else nodo->cabeza_pedidos = cur->siguiente;
               free(cur);
               nodo->stock += cantidad; // Restablecer stock
               return 1;
           }
           prev = cur;
           cur = cur->siguiente;
       }
       return 0;
   }
   
   static int contar_pedidos(Pedido *head) {
       int c = 0;
       while (head) { c++; head = head->siguiente; }
       return c;
   }
   
   // Buscar lote más antiguo por producto (in-order)
   static NodoAVL *buscar_mas_antiguo_por_producto(NodoAVL *raiz, const char *producto) {
       if (!raiz) return NULL;
       NodoAVL *encontrado = buscar_mas_antiguo_por_producto(raiz->izq, producto);
       if (encontrado) return encontrado;
       if (strcmp(raiz->producto, producto) == 0) return raiz;
       return buscar_mas_antiguo_por_producto(raiz->der, producto);
   }
   
   // Reporte in-order
   static void reporte_in_order(NodoAVL *raiz) {
       if (!raiz) return;
       reporte_in_order(raiz->izq);
       printf("Fecha: %d | Producto: %s | Stock: %d | Pedidos en espera: %d\n",
              raiz->fecha, raiz->producto, raiz->stock, contar_pedidos(raiz->cabeza_pedidos));
       reporte_in_order(raiz->der);
   }
   
   // Utilidad para limpiar buffer tras scanf
   static void limpiar_buffer() {
       int c;
       while ((c = getchar()) != '\n' && c != EOF) {}
   }
   
   /* Menú interactivo y flujo principal. */
   int main(void) {
       NodoAVL *raiz = NULL;
       int opcion = -1;
       printf("Sistema Logístico - Puerto de Distribución de Alimentos\n");
       printf("-----------------------------------------------\n");
       do {
           printf("\n Menú:\n");
           printf("1. Recepcion de Mercancia\n");
           printf("2. Registrar Pedido de Despacho\n");
           printf("3. Cancelacion\n");
           printf("4. Reporte de Estado\n");
           printf("0. Salir\n");
           printf("Seleccione una opcion: ");
           if (scanf("%d", &opcion) != 1) { puts("Entrada inválida."); limpiar_buffer(); continue; }
           limpiar_buffer();

           //opcion 1//
   
           if (opcion == 1) {
               // Recepción: insertar lote único y balancear
               int fecha, cantidad;
               char producto[MAX_NOMBRE];
               printf("Ingrese fecha de vencimiento (AAAAMMDD): ");
               if (scanf("%d", &fecha) != 1) { puts("Fecha inválida."); limpiar_buffer(); continue; }
               limpiar_buffer();
               if (buscar_por_fecha(raiz, fecha)) {
                   printf("Error: Ya existe un lote con la misma fecha.\n");
                   continue;
               }
               printf("Ingrese nombre del producto: ");
               if (scanf("%63[^\n]", producto) != 1) { puts("Producto inválido."); limpiar_buffer(); continue; }
               limpiar_buffer();
               printf("Ingrese cantidad (stock total): ");
               if (scanf("%d", &cantidad) != 1) { puts("Cantidad inválida."); limpiar_buffer(); continue; }
               limpiar_buffer();
               int inserted = 0;
               raiz = insertar_avl(raiz, fecha, cantidad, producto, &inserted);
               if (inserted) printf("Lote insertado y árbol balanceado.\n"); else printf("No se insertó (fecha duplicada).\n");
           
            //opcion 2//

            } else if (opcion == 2) {
               // Pedido: encolar en lote más antiguo del producto
               char producto[MAX_NOMBRE];
               char destino[MAX_NOMBRE];
               int cantidad;
               printf("Ingrese producto a despachar: ");
               if (scanf("%63[^\n]", producto) != 1) { puts("Producto inválido."); limpiar_buffer(); continue; }
               limpiar_buffer();
               NodoAVL *nodo = buscar_mas_antiguo_por_producto(raiz, producto);
               if (!nodo) {
                   printf("No hay lotes disponibles del producto '%s'.\n", producto);
                   continue;
               }
               printf("Ingrese destino: ");
               if (scanf("%63[^\n]", destino) != 1) { puts("Destino inválido."); limpiar_buffer(); continue; }
               limpiar_buffer();
               printf("Ingrese cantidad solicitada: ");
               if (scanf("%d", &cantidad) != 1) { puts("Cantidad inválida."); limpiar_buffer(); continue; }
               limpiar_buffer();
               if (cantidad <= 0) { puts("Cantidad debe ser positiva."); continue; }
               if (nodo->stock < cantidad) {
                   printf("Stock insuficiente en lote %d (%s). Disponible: %d\n", nodo->fecha, nodo->producto, nodo->stock);
                   continue;
               }
               if (!encolar_pedido(nodo, destino, cantidad)) {
                   puts("No se pudo registrar el pedido.");
               } else {
                   nodo->stock -= cantidad; // Descuenta stock
                   printf("Pedido registrado en lote %d. Stock restante: %d\n", nodo->fecha, nodo->stock);
               }

               //opcion 3//

           } else if (opcion == 3) {
               // Cancelación: eliminar lote o cancelar pedido
               int sub;
               printf("\nCancelación:\n");
               printf("1. Baja de Producto (Eliminar nodo del árbol)\n");
               printf("2. Cancelar Pedido específico (restablece stock)\n");
               printf("Seleccione opción: ");
               if (scanf("%d", &sub) != 1) { puts("Entrada inválida."); limpiar_buffer(); continue; }
               limpiar_buffer();
               if (sub == 1) {
                   int fecha;
                   printf("Ingrese fecha del lote a eliminar (AAAAMMDD): ");
                   if (scanf("%d", &fecha) != 1) { puts("Fecha inválida."); limpiar_buffer(); continue; }
                   limpiar_buffer();
                   int eliminado = 0;
                   raiz = eliminar_avl(raiz, fecha, &eliminado);
                   if (eliminado) printf("Lote eliminado y árbol balanceado.\n"); else printf("No se encontró la fecha indicada.\n");
               } else if (sub == 2) {
                   int fecha, cantidad;
                   char destino[MAX_NOMBRE];
                   printf("Ingrese fecha del lote donde está el pedido (AAAAMMDD): ");
                   if (scanf("%d", &fecha) != 1) { puts("Fecha inválida."); limpiar_buffer(); continue; }
                   limpiar_buffer();
                   NodoAVL *nodo = buscar_por_fecha(raiz, fecha);
                   if (!nodo) { printf("No existe lote con fecha %d.\n", fecha); continue; }
                   printf("Ingrese destino del pedido a cancelar: ");
                   if (scanf("%63[^\n]", destino) != 1) { puts("Destino inválido."); limpiar_buffer(); continue; }
                   limpiar_buffer();
                   printf("Ingrese cantidad del pedido a cancelar: ");
                   if (scanf("%d", &cantidad) != 1) { puts("Cantidad inválida."); limpiar_buffer(); continue; }
                   limpiar_buffer();
                   if (cancelar_pedido_en_nodo(nodo, destino, cantidad)) {
                       printf("Pedido cancelado. Stock del lote %d ahora: %d\n", nodo->fecha, nodo->stock);
                   } else {
                       printf("No se encontró el pedido indicado en la cola del lote %d.\n", nodo->fecha);
                   }
               } else {
                   puts("Opción de cancelación inválida.");
               }

               //opcion 4//

           } else if (opcion == 4) {
               // Reporte: in-order del inventario
               if (!raiz) {
                   puts("No hay inventario registrado.");
               } else {
                   puts("\nEstado del Inventario (de más próximo a vencer a más lejano):");
                   reporte_in_order(raiz);
               }
           } else if (opcion == 0) {
               puts("Saliendo... liberando memoria.");
           } else {
               puts("Opción inválida.");
           }
       } while (opcion != 0);
   
       liberar_avl(raiz);
       return 0;
   }
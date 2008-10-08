#ifndef _CONNECTION_H_
#define _CONNECTION_H_

struct wl_connection;

#define WL_CONNECTION_READABLE 0x01
#define WL_CONNECTION_WRITABLE 0x02

typedef int (*wl_connection_update_func_t)(struct wl_connection *connection,
					   uint32_t mask, void *data);

struct wl_connection *wl_connection_create(int fd,
					   wl_connection_update_func_t update,
					   void *data);
void wl_connection_destroy(struct wl_connection *connection);
void wl_connection_copy(struct wl_connection *connection, void *data, size_t size);
void wl_connection_consume(struct wl_connection *connection, size_t size);
int wl_connection_data(struct wl_connection *connection, uint32_t mask);
void wl_connection_write(struct wl_connection *connection, const void *data, size_t count);

#endif

#ifndef _CAIRO_UTIL_H
#define _CAIRO_UTIL_H

struct buffer {
	int width, height, stride;
	uint32_t name, handle;
};

struct buffer *
buffer_create(int fd, int width, int height, int stride);

int
buffer_destroy(struct buffer *buffer, int fd);

int
buffer_data(struct buffer *buffer, int fd, void *data);

struct buffer *
buffer_create_from_cairo_surface(int fd, cairo_surface_t *surface);

void
blur_surface(cairo_surface_t *surface);

#endif

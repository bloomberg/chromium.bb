#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <cairo.h>

#include "wayland-client.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

static uint32_t name_cairo_surface(int fd, cairo_surface_t *surface)
{
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	struct drm_i915_gem_pwrite pwrite;
	int32_t width, height, stride;

	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);

	memset(&create, 0, sizeof(create));
	create.size = height * stride;

	if (ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
		fprintf(stderr, "gem create failed: %m\n");
		return 0;
	}

	pwrite.handle = create.handle;
	pwrite.offset = 0;
	pwrite.size = height * stride;
	pwrite.data_ptr = (uint64_t) (uintptr_t)
		cairo_image_surface_get_data(surface);
	if (ioctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite) < 0) {
		fprintf(stderr, "gem pwrite failed: %m\n");
		return 0;
	}

	flink.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
		fprintf(stderr, "gem flink failed: %m\n");
		return 0;
	}

#if 0
	/* We need to hold on to the handle until the server has received
	 * the attach request... we probably need a confirmation event.
	 * I guess the breadcrumb idea will suffice. */
	struct drm_gem_close close;
	close.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close) < 0) {
		fprintf(stderr, "gem close failed: %m\n");
		return 0;
	}
#endif

	return flink.name;
}

static void *
draw_stuff(int width, int height)
{
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     width, height);

	cr = cairo_create(surface);

	cairo_arc(cr, width / 2, height / 2, width / 2 - 10, 0, 2 * M_PI);
	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 0);
	cairo_stroke(cr);

	cairo_arc(cr, width / 2, height / 2, width / 4 - 10, 0, 2 * M_PI);
	cairo_set_source_rgb(cr, 0, 0, 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 0);
	cairo_stroke(cr);

	cairo_destroy(cr);

	return surface;
}

static int
connection_update(struct wl_connection *connection,
		  uint32_t mask, void *data)
{
	struct pollfd *p = data;

	p->events = 0;
	if (mask & WL_CONNECTION_READABLE)
		p->events |= POLLIN;
	if (mask & WL_CONNECTION_WRITABLE)
		p->events |= POLLOUT;

	return 0;
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wl_surface *surface;
	const int x = 200, y = 200, width = 200, height = 200;
	int fd, i, ret;
	uint32_t name, mask;
	cairo_surface_t *s;
	struct pollfd p[1];

	fd = open(gem_device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return -1;
	}

	display = wl_display_create(socket_name,
				    connection_update, &p[0]);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}
	p[0].fd = wl_display_get_fd(display);

	surface = wl_display_create_surface(display);

	s = draw_stuff(width, height);
	name = name_cairo_surface(fd, s);

	wl_surface_attach(surface, name, width, height,
			  cairo_image_surface_get_stride(s));

	i = 0;
	while (ret = poll(p, 1, 20), ret >= 0) {
		if (ret == 0) {
			wl_surface_map(surface, 
				       x + cos(i / 10.0) * 200,
				       y + sin(i / 11.0) * 200,
				       width, height);
			i++;
			continue;
		}

		mask = 0;
		if (p[0].revents & POLLIN)
			mask |= WL_CONNECTION_READABLE;
		if (p[0].revents & POLLOUT)
			mask |= WL_CONNECTION_WRITABLE;
		if (mask)
			wl_display_iterate(display, mask);
	}

	return 0;
}

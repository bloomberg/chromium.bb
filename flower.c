#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>

#include "wayland-client.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

static void
unpremultiply_data(uint8_t *data, int width, int height, int stride)
{
	unsigned int i, j;
	uint8_t *row;

	for (j = 0; j < height; j++) {
		row = data + j * stride;

		for (i = 0; i < width; i++) {
			uint8_t *b = &row[i * 4];
			uint32_t pixel;
			uint8_t  alpha;

			memcpy (&pixel, b, sizeof (uint32_t));
			alpha = (pixel & 0xff000000) >> 24;
			if (alpha == 0) {
				b[0] = b[1] = b[2] = b[3] = 0;
			} else {
				b[0] = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
				b[1] = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
				b[2] = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
				b[3] = alpha;
			}
		}
	}
}

static uint32_t name_cairo_surface(int fd, cairo_surface_t *surface)
{
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	struct drm_i915_gem_pwrite pwrite;
	int32_t width, height, stride;
	void *data;

	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	unpremultiply_data(data, width, height, stride);

	memset(&create, 0, sizeof(create));
	create.size = height * stride;

	if (ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
		fprintf(stderr, "gem create failed: %m\n");
		return 0;
	}

	pwrite.handle = create.handle;
	pwrite.offset = 0;
	pwrite.size = height * stride;
	pwrite.data_ptr = (uint64_t) (uintptr_t) data;
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

static void
set_random_color(cairo_t *cr)
{
	cairo_set_source_rgba(cr,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 100) / 99.0);
}


static void *
draw_stuff(int width, int height)
{
	const int petal_count = 3 + random() % 5;
	const double r1 = 60 + random() % 35;
	const double r2 = 20 + random() % 40;
	const double u = (10 + random() % 90) / 100.0;
	const double v = (random() % 90) / 100.0;

	cairo_surface_t *surface;
	cairo_t *cr;
	int i;
	double t, dt = 2 * M_PI / (petal_count * 2);
	double x1, y1, x2, y2, x3, y3;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     width, height);

	cr = cairo_create(surface);
	cairo_translate(cr, width / 2, height / 2);
	cairo_move_to(cr, cos(t) * r1, sin(t) * r1);
	for (t = 0, i = 0; i < petal_count; i++, t += dt * 2) {
		x1 = cos(t) * r1;
		y1 = sin(t) * r1;
		x2 = cos(t + dt) * r2;
		y2 = sin(t + dt) * r2;
		x3 = cos(t + 2 * dt) * r1;
		y3 = sin(t + 2 * dt) * r1;

		cairo_curve_to(cr,
			       x1 - y1 * u, y1 + x1 * u,
			       x2 + y2 * v, y2 - x2 * v,
			       x2, y2);			       

		cairo_curve_to(cr,
			       x2 - y2 * v, y2 + x2 * v,
			       x3 + y3 * u, y3 - x3 * u,
			       x3, y3);
	}

	cairo_close_path(cr);
	set_random_color(cr);
	cairo_fill_preserve(cr);
	set_random_color(cr);
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
	const int x = 512, y = 384, width = 200, height = 200;
	int fd, i, ret;
	uint32_t name, mask;
	cairo_surface_t *s;
	struct pollfd p[2];
	struct timespec ts;
	struct itimerspec its;
	uint64_t expires;

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

	p[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (p[1].fd < 0) {
		fprintf(stderr, "could not create timerfd\n: %m");
		return -1;
	}

	p[1].events = POLLIN;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	its.it_value = ts;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 20 * 1000000;
	if (timerfd_settime(p[1].fd, TFD_TIMER_ABSTIME, &its, NULL) < 0) {
		fprintf(stderr, "could not set timerfd\n: %m");
		return -1;
	}

	srandom(ts.tv_nsec);

	s = draw_stuff(width, height);
	name = name_cairo_surface(fd, s);

	wl_surface_attach(surface, name, width, height,
			  cairo_image_surface_get_stride(s));

	i = ts.tv_nsec;
	while (ret = poll(p, 2, -1), ret >= 0) {
		if (p[1].revents & POLLIN) {
			read(p[1].fd, &expires, sizeof expires);
			wl_surface_map(surface, 
				       x + cos(i / 31.0) * 400 - width / 2,
				       y + sin(i / 27.0) * 300 - height / 2,
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

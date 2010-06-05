/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>
#include <glib.h>
#include <cairo-drm.h>

#include "wayland-client.h"
#include "wayland-glib.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

static void
set_random_color(cairo_t *cr)
{
	cairo_set_source_rgba(cr,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 100) / 99.0);
}


static void
draw_stuff(cairo_surface_t *surface, int width, int height)
{
	const int petal_count = 3 + random() % 5;
	const double r1 = 60 + random() % 35;
	const double r2 = 20 + random() % 40;
	const double u = (10 + random() % 90) / 100.0;
	const double v = (random() % 90) / 100.0;

	cairo_t *cr;
	int i;
	double t, dt = 2 * M_PI / (petal_count * 2);
	double x1, y1, x2, y2, x3, y3;

	cr = cairo_create(surface);
	cairo_translate(cr, width / 2, height / 2);
	cairo_move_to(cr, cos(0) * r1, sin(0) * r1);
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
}

struct flower {
	struct wl_compositor *compositor;
	struct wl_surface *surface;
	int x, y, width, height;
	int offset;
};

static void
handle_acknowledge(void *data,
			  struct wl_compositor *compositor,
			  uint32_t key, uint32_t frame)
{
}

static void
handle_frame(void *data,
		    struct wl_compositor *compositor,
		    uint32_t frame, uint32_t timestamp)
{
	struct flower *flower = data;

	wl_surface_map(flower->surface, 
		       flower->x + cos((flower->offset + timestamp) / 400.0) * 400 - flower->width / 2,
		       flower->y + sin((flower->offset + timestamp) / 320.0) * 300 - flower->height / 2,
		       flower->width, flower->height);
	wl_compositor_commit(flower->compositor, 0);
}

static const struct wl_compositor_listener compositor_listener = {
	handle_acknowledge,
	handle_frame,
};

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wl_visual *visual;
	int fd;
	cairo_device_t *device;
	cairo_surface_t *s;
	struct timespec ts;
	GMainLoop *loop;
	GSource *source;
	struct flower flower;

	fd = open(gem_device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return -1;
	}

	loop = g_main_loop_new(NULL, FALSE);

	display = wl_display_create(socket_name, sizeof socket_name);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	source = wl_glib_source_new(display);
	g_source_attach(source, NULL);

	/* Process connection events. */
	wl_display_iterate(display, WL_DISPLAY_READABLE);

	flower.compositor = wl_display_get_compositor(display);
	flower.x = 512;
	flower.y = 384;
	flower.width = 200;
	flower.height = 200;
	flower.surface = wl_compositor_create_surface(flower.compositor);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	srandom(ts.tv_nsec);
	flower.offset = random();

	device = cairo_drm_device_get_for_fd(fd);
	s = cairo_drm_surface_create(device,
				     CAIRO_CONTENT_COLOR_ALPHA,
				     flower.width, flower.height);
	draw_stuff(s, flower.width, flower.height);

	visual = wl_display_get_premultiplied_argb_visual(display);
	wl_surface_attach(flower.surface,
			  cairo_drm_surface_get_name(s),
			  flower.width, flower.height,
			  cairo_drm_surface_get_stride(s),
			  visual);

	wl_compositor_add_listener(flower.compositor,
				   &compositor_listener, &flower);

	wl_compositor_commit(flower.compositor, 0);

	g_main_loop_run(loop);

	return 0;
}

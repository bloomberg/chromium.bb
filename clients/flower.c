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

#include "wayland-client.h"
#include "wayland-glib.h"
#include "window.h"

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
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
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
	struct display *display;
	struct window *window;
	int x, y, width, height;
	int offset;
};

static void
frame_callback(void *data, uint32_t time)
{
	struct flower *flower = data;

	window_move(flower->window, 
		    flower->x + cos((flower->offset + time) / 400.0) * 400 - flower->width / 2,
		    flower->y + sin((flower->offset + time) / 320.0) * 300 - flower->height / 2);
	wl_display_frame_callback(display_get_display(flower->display),
				  frame_callback, flower);
}

int main(int argc, char *argv[])
{
	cairo_surface_t *s;
	struct timespec ts;
	struct flower flower;
	struct display *d;

	d = display_create(&argc, &argv, NULL);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	flower.x = 512;
	flower.y = 384;
	flower.width = 200;
	flower.height = 200;
	flower.display = d;
	flower.window = window_create(d, "flower", flower.x, flower.y,
				      flower.width, flower.height);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	srandom(ts.tv_nsec);
	flower.offset = random();

	window_set_decoration(flower.window, 0);
	window_draw(flower.window);
	s = window_get_surface(flower.window);
	if (s == NULL || cairo_surface_status (s) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to create cairo drm surface\n");
		return -1;
	}

	draw_stuff(s, flower.width, flower.height);
	cairo_surface_flush(s);
	cairo_surface_destroy(s);
	window_flush(flower.window);

	window_set_user_data(flower.window, &flower);
	wl_display_frame_callback(display_get_display(d),
				  frame_callback, &flower);

	display_run(d);

	return 0;
}

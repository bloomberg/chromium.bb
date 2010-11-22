/*
 * Copyright © 2010 Kristian Høgsberg
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

struct smoke {
	struct display *display;
	struct window *window;
	cairo_surface_t *surface;
	int x, y, width, height;
	int offset, current;
	struct { float *d, *u, *v; } b[2];
};

static void set_boundary(struct smoke *smoke, float x, float y, float *p)
{
	int i, l;

	l = (smoke->height - 2) * smoke->width;
	for (i = 1; i < smoke->width - 1; i++) {
		p[i] = y * p[i + smoke->width];
		p[l + i + smoke->width] = y * p[l + i];
	}

	for (i = 1; i < smoke->height - 1; i++) {
		p[i * smoke->width] = x * p[i * smoke->width + 1];
		p[i * smoke->width + smoke->width - 1] =
			x * p[i * smoke->width + smoke->width - 2];
	}
}

static void diffuse(struct smoke *smoke, uint32_t time,
		    float *source, float *dest)
{
	float *s, *d;
	int x, y, k, stride;
	float t, a = 0.0002;

	stride = smoke->width;

	for (k = 0; k < 5; k++) {
		for (y = 1; y < smoke->height - 1; y++) {
			s = source + y * stride;
			d = dest + y * stride;
			for (x = 1; x < smoke->width - 1; x++) {
				t = d[x - 1] + d[x + 1] +
					d[x - stride] + d[x + stride];
				d[x] = (s[x] + a * t) / (1 + 4 * a) * 0.995;
			}
		}
	}
}

static void advect(struct smoke *smoke, uint32_t time,
		   float *uu, float *vv, float *source, float *dest)
{
	float *s, *d;
	float *u, *v;
	int x, y, stride;
	int i, j;
	float px, py, fx, fy;

	stride = smoke->width;

	for (y = 1; y < smoke->height - 1; y++) {
		d = dest + y * stride;
		u = uu + y * stride;
		v = vv + y * stride;

		for (x = 1; x < smoke->width - 1; x++) {
			px = x - u[x];
			py = y - v[x];
			if (px < 0.5)
				px = 0.5;
			if (py < 0.5)
				py = 0.5;
			if (px > smoke->width - 0.5)
				px = smoke->width - 0.5;
			if (py > smoke->height - 0.5)
				py = smoke->height - 0.5;
			i = (int) px;
			j = (int) py;
			fx = px - i;
			fy = py - j;
			s = source + j * stride + i;
			d[x] = (s[0] * (1 - fx) + s[1] * fx) * (1 - fy) +
				(s[stride] * (1 - fx) + s[stride + 1] * fx) * fy;
		}
	}
}

static void project(struct smoke *smoke, uint32_t time,
		    float *u, float *v, float *p, float *div)
{
	int x, y, k, l, s;
	float h;

	h = 1.0 / smoke->width;
	s = smoke->width;
	memset(p, 0, smoke->height * smoke->width);
	for (y = 1; y < smoke->height - 1; y++) {
		l = y * s;
		for (x = 1; x < smoke->width - 1; x++) {
			div[l + x] = -0.5 * h * (u[l + x + 1] - u[l + x - 1] +
						 v[l + x + s] - v[l + x - s]);
			p[l + x] = 0;
		}
	}

	for (k = 0; k < 5; k++) {
		for (y = 1; y < smoke->height - 1; y++) {
			l = y * s;
			for (x = 1; x < smoke->width - 1; x++) {
				p[l + x] = (div[l + x] +
					    p[l + x - 1] +
					    p[l + x + 1] +
					    p[l + x - s] +
					    p[l + x + s]) / 4;
			}
		}
	}

	for (y = 1; y < smoke->height - 1; y++) {
		l = y * s;
		for (x = 1; x < smoke->width - 1; x++) {
			u[l + x] -= 0.5 * (p[l + x + 1] - p[l + x - 1]) / h;
			v[l + x] -= 0.5 * (p[l + x + s] - p[l + x - s]) / h;
		}
	}
}

static void render(struct smoke *smoke)
{
	unsigned char *dest;
	int x, y, width, height, stride;
	float *s;
	uint32_t *d, c, a;

	dest = cairo_image_surface_get_data (smoke->surface);
	width = cairo_image_surface_get_width (smoke->surface);
	height = cairo_image_surface_get_height (smoke->surface);
	stride = cairo_image_surface_get_stride (smoke->surface);

	for (y = 1; y < height - 1; y++) {
		s = smoke->b[smoke->current].d + y * smoke->height;
		d = (uint32_t *) (dest + y * stride);
		for (x = 1; x < width - 1; x++) {
			c = (int) (s[x] * 800);
			if (c > 255)
				c = 255;
			a = c;
			if (a < 0x33)
				a = 0x33;
			d[x] = (a << 24) | (c << 16) | (c << 8) | c;
		}
	}
}

static void
frame_callback(void *data, uint32_t time)
{
	struct smoke *smoke = data;

	diffuse(smoke, time / 30, smoke->b[0].u, smoke->b[1].u);
	diffuse(smoke, time / 30, smoke->b[0].v, smoke->b[1].v);
	project(smoke, time / 30,
		smoke->b[1].u, smoke->b[1].v,
		smoke->b[0].u, smoke->b[0].v);
	advect(smoke, time / 30,
	       smoke->b[1].u, smoke->b[1].v,
	       smoke->b[1].u, smoke->b[0].u);
	advect(smoke, time / 30,
	       smoke->b[1].u, smoke->b[1].v,
	       smoke->b[1].v, smoke->b[0].v);
	project(smoke, time / 30,
		smoke->b[0].u, smoke->b[0].v,
		smoke->b[1].u, smoke->b[1].v);

	diffuse(smoke, time / 30, smoke->b[0].d, smoke->b[1].d);
	advect(smoke, time / 30,
	       smoke->b[0].u, smoke->b[0].v,
	       smoke->b[1].d, smoke->b[0].d);

	render(smoke);

	window_damage(smoke->window, 0, 0, smoke->width, smoke->height);
	wl_display_frame_callback(display_get_display(smoke->display),
				  frame_callback, smoke);
}

static int
smoke_motion_handler(struct window *window,
		     struct input *input, uint32_t time,
		     int32_t x, int32_t y,
		     int32_t surface_x, int32_t surface_y, void *data)
{
	struct smoke *smoke = data;
	int i, i0, i1, j, j0, j1, k, d = 5;

	if (surface_x - d < 1)
		i0 = 1;
	else
		i0 = surface_x - d;
	if (i0 + 2 * d > smoke->width - 1)
		i1 = smoke->width - 1;
	else
		i1 = i0 + 2 * d;

	if (surface_y - d < 1)
		j0 = 1;
	else
		j0 = surface_y - d;
	if (j0 + 2 * d > smoke->height - 1)
		j1 = smoke->height - 1;
	else
		j1 = j0 + 2 * d;

	for (i = i0; i < i1; i++)
		for (j = j0; j < j1; j++) {
			k = j * smoke->width + i;
			smoke->b[0].u[k] += 256 - (random() & 512);
			smoke->b[0].v[k] += 256 - (random() & 512);
			smoke->b[0].d[k] += 1;
		}

	return POINTER_HAND1;
}

int main(int argc, char *argv[])
{
	struct timespec ts;
	struct smoke smoke;
	struct display *d;
	int size;

	d = display_create(&argc, &argv, NULL);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	smoke.x = 200;
	smoke.y = 200;
	smoke.width = 200;
	smoke.height = 200;
	smoke.display = d;
	smoke.window = window_create(d, "smoke", smoke.x, smoke.y,
				      smoke.width, smoke.height);

	window_set_buffer_type(smoke.window, WINDOW_BUFFER_TYPE_SHM);
	clock_gettime(CLOCK_MONOTONIC, &ts);
	srandom(ts.tv_nsec);
	smoke.offset = random();

	smoke.current = 0;
	size = smoke.height * smoke.width;
	smoke.b[0].d = calloc(size, sizeof(float));
	smoke.b[0].u = calloc(size, sizeof(float));
	smoke.b[0].v = calloc(size, sizeof(float));
	smoke.b[1].d = calloc(size, sizeof(float));
	smoke.b[1].u = calloc(size, sizeof(float));
	smoke.b[1].v = calloc(size, sizeof(float));

	window_set_decoration(smoke.window, 0);
	window_create_surface(smoke.window);
	smoke.surface = window_get_surface(smoke.window);

	window_flush(smoke.window);

	window_set_motion_handler(smoke.window,
				  smoke_motion_handler);

	window_set_user_data(smoke.window, &smoke);
	wl_display_frame_callback(display_get_display(d),
				  frame_callback, &smoke);

	display_run(d);

	return 0;
}

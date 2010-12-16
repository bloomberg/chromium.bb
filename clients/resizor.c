/*
 * Copyright © 2010 Intel Corporation
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
#include <cairo.h>
#include <math.h>

#include "wayland-util.h"
#include "wayland-client.h"
#include "wayland-glib.h"

#include "window.h"

#include <X11/keysym.h>

struct resizor {
	struct display *display;
	struct window *window;
	struct rectangle child_allocation;

	struct {
		double current;
		double target;
		double previous;
	} height;
};

static void
resizor_draw(struct resizor *resizor)
{
	cairo_surface_t *surface;
	cairo_t *cr;

	window_draw(resizor->window);

	window_get_child_rectangle(resizor->window,
				   &resizor->child_allocation);

	surface = window_get_surface(resizor->window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle(cr,
			resizor->child_allocation.x,
			resizor->child_allocation.y,
			resizor->child_allocation.width,
			resizor->child_allocation.height);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
	cairo_fill(cr);
	cairo_destroy(cr);

	cairo_surface_destroy(surface);

	window_flush(resizor->window);

	if (fabs(resizor->height.previous - resizor->height.target) < 0.1) {
		wl_display_frame_callback(display_get_display(resizor->display),
					  frame_callback, resizor);
	}
}

static void
redraw_handler(struct window *window, void *data)
{
	struct resizor *resizor = data;

	resizor_draw(resizor);
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct resizor *resizor = data;

	window_schedule_redraw(resizor->window);
}

static void
frame_callback(void *data, uint32_t time)
{
	struct resizor *resizor = data;
	double force, height;

	height = resizor->height.current;
	force = (resizor->height.target - height) / 10.0 +
		(resizor->height.previous - height);

	resizor->height.current =
		height + (height - resizor->height.previous) + force;
	resizor->height.previous = height;

	if (resizor->height.current >= 400) {
		resizor->height.current = 400;
		resizor->height.previous = 400;
	}

	if (resizor->height.current <= 200) {
		resizor->height.current = 200;
		resizor->height.previous = 200;
	}

	resizor->child_allocation.height = height + 0.5;
	window_set_child_size(resizor->window,
			      &resizor->child_allocation);

	window_schedule_redraw(resizor->window);
}

static void
key_handler(struct window *window, uint32_t key, uint32_t sym,
	    uint32_t state, uint32_t modifiers, void *data)
{
	struct resizor *resizor = data;

	if (state == 0)
		return;

	switch (sym) {
	case XK_F1:
		resizor->height.target = 400;
		resizor->height.current = resizor->child_allocation.height;
		frame_callback(resizor, 0);
		break;
	case XK_F2:
		resizor->height.target = 200;
		resizor->height.current = resizor->child_allocation.height;
		frame_callback(resizor, 0);
		break;
	}
}

static struct resizor *
resizor_create(struct display *display)
{
	struct resizor *resizor;

	resizor = malloc(sizeof *resizor);
	if (resizor == NULL)
		return resizor;
	memset(resizor, 0, sizeof *resizor);

	resizor->window = window_create(display, "Wayland Resizor",
				     100, 100, 500, 400);
	resizor->display = display;

	window_set_key_handler(resizor->window, key_handler);
	window_set_user_data(resizor->window, resizor);
	window_set_redraw_handler(resizor->window, redraw_handler);
	window_set_keyboard_focus_handler(resizor->window,
					  keyboard_focus_handler);

	resizor->child_allocation.x = 0;
	resizor->child_allocation.y = 0;
	resizor->child_allocation.width = 300;
	resizor->child_allocation.height = 400;
	resizor->height.current = 400;
	resizor->height.previous = 400;
	resizor->height.target = 400;

	window_set_child_size(resizor->window, &resizor->child_allocation);

	resizor_draw(resizor);

	return resizor;
}

int
main(int argc, char *argv[])
{
	struct display *d;

	d = display_create(&argc, &argv, NULL);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	resizor_create (d);

	display_run(d);

	return 0;
}

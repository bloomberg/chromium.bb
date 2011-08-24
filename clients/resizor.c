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
	struct window *menu;
	int32_t width;

	struct {
		double current;
		double target;
		double previous;
	} height;
};

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	static const struct wl_callback_listener listener = {
		frame_callback
	};
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

	window_set_child_size(resizor->window, resizor->width, height + 0.5);

	window_schedule_redraw(resizor->window);

	if (callback)
		wl_callback_destroy(callback);

	if (fabs(resizor->height.previous - resizor->height.target) > 0.1) {
		callback = wl_surface_frame(window_get_wl_surface(resizor->window));
		wl_callback_add_listener(callback, &listener, resizor);
	}
}

static void
resizor_draw(struct resizor *resizor)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	struct rectangle allocation;

	window_draw(resizor->window);

	window_get_child_allocation(resizor->window, &allocation);

	surface = window_get_surface(resizor->window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle(cr,
			allocation.x,
			allocation.y,
			allocation.width,
			allocation.height);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
	cairo_fill(cr);
	cairo_destroy(cr);

	cairo_surface_destroy(surface);

	window_flush(resizor->window);
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
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym, uint32_t state, void *data)
{
	struct resizor *resizor = data;

	if (state == 0)
		return;

	switch (sym) {
	case XK_Down:
		resizor->height.target = 400;
		frame_callback(resizor, NULL, 0);
		break;
	case XK_Up:
		resizor->height.target = 200;
		frame_callback(resizor, NULL, 0);
		break;
	}
}

static void
show_menu(struct resizor *resizor, struct input *input)
{
	int32_t x, y, width = 200, height = 200;

	input_get_position(input, &x, &y);
	resizor->menu = window_create_transient(resizor->display,
						resizor->window,
						x - 10, y - 10, width, height);

	window_draw(resizor->menu);
	window_flush(resizor->menu);
}

static void
button_handler(struct window *window,
	       struct input *input, uint32_t time,
	       int button, int state, void *data)
{
	struct resizor *resizor = data;

	switch (button) {
	case 274:
		if (state)
			show_menu(resizor, input);
		else
			window_destroy(resizor->menu);
		break;
	}
}

static struct resizor *
resizor_create(struct display *display)
{
	struct resizor *resizor;
	int32_t height;

	resizor = malloc(sizeof *resizor);
	if (resizor == NULL)
		return resizor;
	memset(resizor, 0, sizeof *resizor);

	resizor->window = window_create(display, 500, 400);
	window_set_title(resizor->window, "Wayland Resizor");
	resizor->display = display;

	window_set_key_handler(resizor->window, key_handler);
	window_set_user_data(resizor->window, resizor);
	window_set_redraw_handler(resizor->window, redraw_handler);
	window_set_keyboard_focus_handler(resizor->window,
					  keyboard_focus_handler);

	resizor->width = 300;
	resizor->height.current = 400;
	resizor->height.previous = resizor->height.current;
	resizor->height.target = resizor->height.current;
	height = resizor->height.current + 0.5;

	window_set_child_size(resizor->window, resizor->width, height);
	window_set_button_handler(resizor->window, button_handler);

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

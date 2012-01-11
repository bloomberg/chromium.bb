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
#include <assert.h>

#include <linux/input.h>
#include <wayland-client.h>

#include "window.h"

#include <X11/keysym.h>

struct resizor {
	struct display *display;
	struct window *window;
	struct widget *widget;
	struct window *menu;
	int32_t width;

	struct {
		double current;
		double target;
		double previous;
	} height;
	struct wl_callback *frame_callback;
};

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	struct resizor *resizor = data;
	double force, height;

	assert(!callback || callback == resizor->frame_callback);

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

	widget_schedule_resize(resizor->widget, resizor->width, height + 0.5);

	if (resizor->frame_callback) {
		wl_callback_destroy(resizor->frame_callback);
		resizor->frame_callback = NULL;
	}
}

static const struct wl_callback_listener listener = {
	frame_callback
};

static void
redraw_handler(struct widget *widget, void *data)
{
	struct resizor *resizor = data;
	cairo_surface_t *surface;
	cairo_t *cr;
	struct rectangle allocation;

	widget_get_allocation(resizor->widget, &allocation);

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

	if (fabs(resizor->height.previous - resizor->height.target) > 0.1) {
		resizor->frame_callback =
			wl_surface_frame(
				window_get_wl_surface(resizor->window));
		wl_callback_add_listener(resizor->frame_callback, &listener,
					 resizor);
	}

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
	case XK_Escape:
		display_exit(resizor->display);
		break;
	}
}

static void
menu_func(struct window *window, int index, void *user_data)
{
	fprintf(stderr, "picked entry %d\n", index);
}

static void
show_menu(struct resizor *resizor, struct input *input, uint32_t time)
{
	int32_t x, y;
	static const char *entries[] = {
		"Roy", "Pris", "Leon", "Zhora"
	};

	input_get_position(input, &x, &y);
	resizor->menu = window_create_menu(resizor->display,
					   input, time, resizor->window,
					   x - 10, y - 10,
					   menu_func, entries, 4);

	window_schedule_redraw(resizor->menu);
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       int button, int state, void *data)
{
	struct resizor *resizor = data;

	switch (button) {
	case BTN_RIGHT:
		if (state)
			show_menu(resizor, input, time);
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
	resizor->widget = frame_create(resizor->window, resizor);
	window_set_title(resizor->window, "Wayland Resizor");
	resizor->display = display;

	window_set_key_handler(resizor->window, key_handler);
	window_set_user_data(resizor->window, resizor);
	widget_set_redraw_handler(resizor->widget, redraw_handler);
	window_set_keyboard_focus_handler(resizor->window,
					  keyboard_focus_handler);

	resizor->width = 300;
	resizor->height.current = 400;
	resizor->height.previous = resizor->height.current;
	resizor->height.target = resizor->height.current;
	height = resizor->height.current + 0.5;

	widget_set_button_handler(resizor->widget, button_handler);

	widget_schedule_resize(resizor->widget, resizor->width, height);

	return resizor;
}

static void
resizor_destroy(struct resizor *resizor)
{
	if (resizor->frame_callback)
		wl_callback_destroy(resizor->frame_callback);

	window_destroy(resizor->window);
	free(resizor);
}

int
main(int argc, char *argv[])
{
	struct display *display;
	struct resizor *resizor;

	display = display_create(&argc, &argv, NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	resizor = resizor_create(display);

	display_run(display);

	resizor_destroy(resizor);
	display_destroy(display);

	return 0;
}

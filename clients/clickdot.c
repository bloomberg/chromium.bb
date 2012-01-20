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

struct clickdot {
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
	struct clickdot *clickdot = data;
	double force, height;

	assert(!callback || callback == clickdot->frame_callback);

	height = clickdot->height.current;
	force = (clickdot->height.target - height) / 10.0 +
		(clickdot->height.previous - height);

	clickdot->height.current =
		height + (height - clickdot->height.previous) + force;
	clickdot->height.previous = height;

	if (clickdot->height.current >= 400) {
		clickdot->height.current = 400;
		clickdot->height.previous = 400;
	}

	if (clickdot->height.current <= 200) {
		clickdot->height.current = 200;
		clickdot->height.previous = 200;
	}

	widget_schedule_resize(clickdot->widget, clickdot->width, height + 0.5);

	if (clickdot->frame_callback) {
		wl_callback_destroy(clickdot->frame_callback);
		clickdot->frame_callback = NULL;
	}
}

static const struct wl_callback_listener listener = {
	frame_callback
};

static void
redraw_handler(struct widget *widget, void *data)
{
	struct clickdot *clickdot = data;
	cairo_surface_t *surface;
	cairo_t *cr;
	struct rectangle allocation;

	widget_get_allocation(clickdot->widget, &allocation);

	surface = window_get_surface(clickdot->window);

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

	if (fabs(clickdot->height.previous - clickdot->height.target) > 0.1) {
		clickdot->frame_callback =
			wl_surface_frame(
				window_get_wl_surface(clickdot->window));
		wl_callback_add_listener(clickdot->frame_callback, &listener,
					 clickdot);
	}

}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct clickdot *clickdot = data;

	window_schedule_redraw(clickdot->window);
}

static void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym, uint32_t state, void *data)
{
	struct clickdot *clickdot = data;

	if (state == 0)
		return;

	switch (sym) {
	case XK_Down:
		clickdot->height.target = 400;
		frame_callback(clickdot, NULL, 0);
		break;
	case XK_Up:
		clickdot->height.target = 200;
		frame_callback(clickdot, NULL, 0);
		break;
	case XK_Escape:
		display_exit(clickdot->display);
		break;
	}
}

static void
menu_func(struct window *window, int index, void *user_data)
{
	fprintf(stderr, "picked entry %d\n", index);
}

static void
show_menu(struct clickdot *clickdot, struct input *input, uint32_t time)
{
	int32_t x, y;
	static const char *entries[] = {
		"Roy", "Pris", "Leon", "Zhora"
	};

	input_get_position(input, &x, &y);
	window_show_menu(clickdot->display, input, time, clickdot->window,
			 x - 10, y - 10, menu_func, entries, 4);
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       int button, int state, void *data)
{
	struct clickdot *clickdot = data;

	switch (button) {
	case BTN_RIGHT:
		if (state)
			show_menu(clickdot, input, time);
		break;
	}
}

static struct clickdot *
clickdot_create(struct display *display)
{
	struct clickdot *clickdot;
	int32_t height;

	clickdot = malloc(sizeof *clickdot);
	if (clickdot == NULL)
		return clickdot;
	memset(clickdot, 0, sizeof *clickdot);

	clickdot->window = window_create(display, 500, 400);
	clickdot->widget = frame_create(clickdot->window, clickdot);
	window_set_title(clickdot->window, "Wayland Resizor");
	clickdot->display = display;

	window_set_key_handler(clickdot->window, key_handler);
	window_set_user_data(clickdot->window, clickdot);
	widget_set_redraw_handler(clickdot->widget, redraw_handler);
	window_set_keyboard_focus_handler(clickdot->window,
					  keyboard_focus_handler);

	clickdot->width = 300;
	clickdot->height.current = 400;
	clickdot->height.previous = clickdot->height.current;
	clickdot->height.target = clickdot->height.current;
	height = clickdot->height.current + 0.5;

	widget_set_button_handler(clickdot->widget, button_handler);

	widget_schedule_resize(clickdot->widget, clickdot->width, height);

	return clickdot;
}

static void
clickdot_destroy(struct clickdot *clickdot)
{
	if (clickdot->frame_callback)
		wl_callback_destroy(clickdot->frame_callback);

	widget_destroy(clickdot->widget);
	window_destroy(clickdot->window);
	free(clickdot);
}

int
main(int argc, char *argv[])
{
	struct display *display;
	struct clickdot *clickdot;

	display = display_create(&argc, &argv, NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	clickdot = clickdot_create(display);

	display_run(display);

	clickdot_destroy(clickdot);
	display_destroy(display);

	return 0;
}

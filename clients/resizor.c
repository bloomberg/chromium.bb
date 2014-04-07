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

#include "config.h"

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

struct spring {
	double current;
	double target;
	double previous;
};

struct resizor {
	struct display *display;
	struct window *window;
	struct widget *widget;
	struct window *menu;
	struct spring width;
	struct spring height;
	struct wl_callback *frame_callback;
};

static void
spring_update(struct spring *spring)
{
	double current, force;

	current = spring->current;
	force = (spring->target - current) / 20.0 +
		(spring->previous - current);

	spring->current = current + (current - spring->previous) + force;
	spring->previous = current;
}

static int
spring_done(struct spring *spring)
{
	return fabs(spring->previous - spring->target) < 0.1;
}

static const struct wl_callback_listener listener;

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	struct resizor *resizor = data;

	assert(!callback || callback == resizor->frame_callback);

	if (resizor->frame_callback) {
		wl_callback_destroy(resizor->frame_callback);
		resizor->frame_callback = NULL;
	}

	if (window_is_maximized(resizor->window))
		return;

	spring_update(&resizor->width);
	spring_update(&resizor->height);

	widget_schedule_resize(resizor->widget,
			       resizor->width.current + 0.5,
			       resizor->height.current + 0.5);

	if (!spring_done(&resizor->width) || !spring_done(&resizor->height)) {
		resizor->frame_callback =
			wl_surface_frame(
				window_get_wl_surface(resizor->window));
		wl_callback_add_listener(resizor->frame_callback, &listener,
					 resizor);
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
	    uint32_t key, uint32_t sym, enum wl_keyboard_key_state state,
	    void *data)
{
	struct resizor *resizor = data;
	struct rectangle allocation;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	window_get_allocation(resizor->window, &allocation);
	resizor->width.current = allocation.width;
	if (spring_done(&resizor->width))
		resizor->width.target = allocation.width;
	resizor->height.current = allocation.height;
	if (spring_done(&resizor->height))
		resizor->height.target = allocation.height;

	switch (sym) {
	case XKB_KEY_Up:
		if (allocation.height < 400)
			break;

		resizor->height.target = allocation.height - 200;
		break;

	case XKB_KEY_Down:
		if (allocation.height > 1000)
			break;

		resizor->height.target = allocation.height + 200;
		break;

	case XKB_KEY_Left:
		if (allocation.width < 400)
			break;

		resizor->width.target = allocation.width - 200;
		break;

	case XKB_KEY_Right:
		if (allocation.width > 1000)
			break;

		resizor->width.target = allocation.width + 200;
		break;

	case XKB_KEY_Escape:
		display_exit(resizor->display);
		break;
	}

	if (!resizor->frame_callback)
		frame_callback(resizor, NULL, 0);
}

static void
menu_func(struct window *window,
	  struct input *input, int index, void *user_data)
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
	window_show_menu(resizor->display, input, time, resizor->window,
			 x - 10, y - 10, menu_func, entries, 4);
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button, enum wl_pointer_button_state state, void *data)
{
	struct resizor *resizor = data;

	switch (button) {
	case BTN_RIGHT:
		if (state == WL_POINTER_BUTTON_STATE_PRESSED)
			show_menu(resizor, input, time);
		break;
	}
}

static struct resizor *
resizor_create(struct display *display)
{
	struct resizor *resizor;

	resizor = xzalloc(sizeof *resizor);
	resizor->window = window_create(display);
	resizor->widget = window_frame_create(resizor->window, resizor);
	window_set_title(resizor->window, "Wayland Resizor");
	resizor->display = display;

	window_set_key_handler(resizor->window, key_handler);
	window_set_user_data(resizor->window, resizor);
	widget_set_redraw_handler(resizor->widget, redraw_handler);
	window_set_keyboard_focus_handler(resizor->window,
					  keyboard_focus_handler);

	widget_set_button_handler(resizor->widget, button_handler);

	resizor->height.previous = 400;
	resizor->height.current = 400;
	resizor->height.target = 400;

	resizor->width.previous = 400;
	resizor->width.current = 400;
	resizor->width.target = 400;

	widget_schedule_resize(resizor->widget, 400, 400);

	return resizor;
}

static void
resizor_destroy(struct resizor *resizor)
{
	if (resizor->frame_callback)
		wl_callback_destroy(resizor->frame_callback);

	widget_destroy(resizor->widget);
	window_destroy(resizor->window);
	free(resizor);
}

int
main(int argc, char *argv[])
{
	struct display *display;
	struct resizor *resizor;

	display = display_create(&argc, argv);
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

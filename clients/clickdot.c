/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2012 Collabora, Ltd.
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

	int32_t x, y;
};

static void
redraw_handler(struct widget *widget, void *data)
{
	static const double r = 10.0;
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

	cairo_translate(cr, clickdot->x + 0.5, clickdot->y + 0.5);
	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgb(cr, 0.1, 0.9, 0.9);
	cairo_move_to(cr, 0.0, -r);
	cairo_line_to(cr, 0.0, r);
	cairo_move_to(cr, -r, 0.0);
	cairo_line_to(cr, r, 0.0);
	cairo_arc(cr, 0.0, 0.0, r, 0.0, 2.0 * M_PI);
	cairo_stroke(cr);

	cairo_destroy(cr);

	cairo_surface_destroy(surface);
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
	case XK_Escape:
		display_exit(clickdot->display);
		break;
	}
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button, uint32_t state, void *data)
{
	struct clickdot *clickdot = data;

	if (state && button == BTN_LEFT)
		input_get_position(input, &clickdot->x, &clickdot->y);

	widget_schedule_redraw(widget);
}

static struct clickdot *
clickdot_create(struct display *display)
{
	struct clickdot *clickdot;

	clickdot = malloc(sizeof *clickdot);
	if (clickdot == NULL)
		return clickdot;
	memset(clickdot, 0, sizeof *clickdot);

	clickdot->window = window_create(display);
	clickdot->widget = frame_create(clickdot->window, clickdot);
	window_set_title(clickdot->window, "Wayland ClickDot");
	clickdot->display = display;

	window_set_key_handler(clickdot->window, key_handler);
	window_set_user_data(clickdot->window, clickdot);
	window_set_keyboard_focus_handler(clickdot->window,
					  keyboard_focus_handler);

	widget_set_redraw_handler(clickdot->widget, redraw_handler);
	widget_set_button_handler(clickdot->widget, button_handler);

	widget_schedule_resize(clickdot->widget, 500, 400);
	clickdot->x = 250;
	clickdot->y = 200;

	return clickdot;
}

static void
clickdot_destroy(struct clickdot *clickdot)
{
	widget_destroy(clickdot->widget);
	window_destroy(clickdot->window);
	free(clickdot);
}

int
main(int argc, char *argv[])
{
	struct display *display;
	struct clickdot *clickdot;

	display = display_create(argc, argv);
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

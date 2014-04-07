/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2012 Collabora, Ltd.
 * Copyright © 2012 Jonas Ådahl
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

struct clickdot {
	struct display *display;
	struct window *window;
	struct widget *widget;

	cairo_surface_t *buffer;

	struct {
		int32_t x, y;
	} dot;

	struct {
		int32_t x, y;
		int32_t old_x, old_y;
	} line;

	int reset;
};

static void
draw_line(struct clickdot *clickdot, cairo_t *cr,
	  struct rectangle *allocation)
{
	cairo_t *bcr;
	cairo_surface_t *tmp_buffer = NULL;

	if (clickdot->reset) {
		tmp_buffer = clickdot->buffer;
		clickdot->buffer = NULL;
		clickdot->line.x = -1;
		clickdot->line.y = -1;
		clickdot->line.old_x = -1;
		clickdot->line.old_y = -1;
		clickdot->reset = 0;
	}

	if (clickdot->buffer == NULL) {
		clickdot->buffer =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						   allocation->width,
						   allocation->height);
		bcr = cairo_create(clickdot->buffer);
		cairo_set_source_rgba(bcr, 0, 0, 0, 0);
		cairo_rectangle(bcr,
				0, 0,
				allocation->width, allocation->height);
		cairo_fill(bcr);
	}
	else
		bcr = cairo_create(clickdot->buffer);

	if (tmp_buffer) {
		cairo_set_source_surface(bcr, tmp_buffer, 0, 0);
		cairo_rectangle(bcr, 0, 0,
				allocation->width, allocation->height);
		cairo_clip(bcr);
		cairo_paint(bcr);

		cairo_surface_destroy(tmp_buffer);
	}

	if (clickdot->line.x != -1 && clickdot->line.y != -1) {
		if (clickdot->line.old_x != -1 &&
		    clickdot->line.old_y != -1) {
			cairo_set_line_width(bcr, 2.0);
			cairo_set_source_rgb(bcr, 1, 1, 1);
			cairo_translate(bcr,
					-allocation->x, -allocation->y);

			cairo_move_to(bcr,
				      clickdot->line.old_x,
				      clickdot->line.old_y);
			cairo_line_to(bcr,
				      clickdot->line.x,
				      clickdot->line.y);

			cairo_stroke(bcr);
		}

		clickdot->line.old_x = clickdot->line.x;
		clickdot->line.old_y = clickdot->line.y;
	}
	cairo_destroy(bcr);

	cairo_set_source_surface(cr, clickdot->buffer,
				 allocation->x, allocation->y);
	cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
	cairo_rectangle(cr,
			allocation->x, allocation->y,
			allocation->width, allocation->height);
	cairo_clip(cr);
	cairo_paint(cr);
}

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

	draw_line(clickdot, cr, &allocation);

	cairo_translate(cr, clickdot->dot.x + 0.5, clickdot->dot.y + 0.5);
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
	    uint32_t key, uint32_t sym,
	    enum wl_keyboard_key_state state, void *data)
{
	struct clickdot *clickdot = data;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	switch (sym) {
	case XKB_KEY_Escape:
		display_exit(clickdot->display);
		break;
	}
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button,
	       enum wl_pointer_button_state state, void *data)
{
	struct clickdot *clickdot = data;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT)
		input_get_position(input, &clickdot->dot.x, &clickdot->dot.y);

	widget_schedule_redraw(widget);
}

static int
motion_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       float x, float y, void *data)
{
	struct clickdot *clickdot = data;
	clickdot->line.x = x;
	clickdot->line.y = y;

	window_schedule_redraw(clickdot->window);

	return CURSOR_LEFT_PTR;
}

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height,
	       void *data)
{
	struct clickdot *clickdot = data;

	clickdot->reset = 1;
}

static void
leave_handler(struct widget *widget,
	      struct input *input, void *data)
{
	struct clickdot *clickdot = data;

	clickdot->reset = 1;
}

static struct clickdot *
clickdot_create(struct display *display)
{
	struct clickdot *clickdot;

	clickdot = xzalloc(sizeof *clickdot);
	clickdot->window = window_create(display);
	clickdot->widget = window_frame_create(clickdot->window, clickdot);
	window_set_title(clickdot->window, "Wayland ClickDot");
	clickdot->display = display;
	clickdot->buffer = NULL;

	window_set_key_handler(clickdot->window, key_handler);
	window_set_user_data(clickdot->window, clickdot);
	window_set_keyboard_focus_handler(clickdot->window,
					  keyboard_focus_handler);

	widget_set_redraw_handler(clickdot->widget, redraw_handler);
	widget_set_button_handler(clickdot->widget, button_handler);
	widget_set_motion_handler(clickdot->widget, motion_handler);
	widget_set_resize_handler(clickdot->widget, resize_handler);
	widget_set_leave_handler(clickdot->widget, leave_handler);

	widget_schedule_resize(clickdot->widget, 500, 400);
	clickdot->dot.x = 250;
	clickdot->dot.y = 200;
	clickdot->line.x = -1;
	clickdot->line.y = -1;
	clickdot->line.old_x = -1;
	clickdot->line.old_y = -1;
	clickdot->reset = 0;

	return clickdot;
}

static void
clickdot_destroy(struct clickdot *clickdot)
{
	if (clickdot->buffer)
		cairo_surface_destroy(clickdot->buffer);
	widget_destroy(clickdot->widget);
	window_destroy(clickdot->window);
	free(clickdot);
}

int
main(int argc, char *argv[])
{
	struct display *display;
	struct clickdot *clickdot;

	display = display_create(&argc, argv);
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

/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013 Collabora, Ltd.
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

#include <stdio.h>
#include <string.h>
#include <cairo.h>

#include <linux/input.h>

#include "window.h"
#include "scaler-client-protocol.h"

#define BUFFER_SCALE 2
static const int BUFFER_WIDTH = 421 * BUFFER_SCALE;
static const int BUFFER_HEIGHT = 337 * BUFFER_SCALE;
static const int SURFACE_WIDTH = 55 * 4;
static const int SURFACE_HEIGHT = 77 * 4;
static const double RECT_X = 21 * BUFFER_SCALE; /* buffer coords */
static const double RECT_Y = 25 * BUFFER_SCALE;
static const double RECT_W = 55 * BUFFER_SCALE;
static const double RECT_H = 77 * BUFFER_SCALE;

struct box {
	struct display *display;
	struct window *window;
	struct widget *widget;
	int width, height;

	struct wl_scaler *scaler;
	struct wl_viewport *viewport;
};

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height, void *data)
{
	struct box *box = data;

	/* Dont resize me */
	widget_set_size(box->widget, box->width, box->height);
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct box *box = data;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = window_get_surface(box->window);
	if (surface == NULL ||
	    cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to create cairo egl surface\n");
		return;
	}

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_line_width(cr, 1.0);
	cairo_translate(cr, RECT_X, RECT_Y);

	/* red background */
	cairo_set_source_rgba(cr, 255, 0, 0, 255);
	cairo_paint(cr);

	/* blue box */
	cairo_set_source_rgba(cr, 0, 0, 255, 255);
	cairo_rectangle(cr, 0, 0, RECT_W, RECT_H);
	cairo_fill(cr);

	/* black border outside the box */
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to(cr, 0, RECT_H + 0.5);
	cairo_line_to(cr, RECT_W, RECT_H + 0.5);
	cairo_stroke(cr);

	/* white border inside the box */
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_move_to(cr, RECT_W - 0.5, 0);
	cairo_line_to(cr, RECT_W - 0.5, RECT_H);
	cairo_stroke(cr);

	/* the green border on inside the box, to be split half by crop */
	cairo_set_source_rgb(cr, 0, 1, 0);
	cairo_move_to(cr, 0.5, RECT_H);
	cairo_line_to(cr, 0.5, 0);
	cairo_move_to(cr, 0, 0.5);
	cairo_line_to(cr, RECT_W, 0.5);
	cairo_stroke(cr);

	cairo_destroy(cr);

	/* TODO: buffer_transform */

	cairo_surface_destroy(surface);
}

static void
global_handler(struct display *display, uint32_t name,
	       const char *interface, uint32_t version, void *data)
{
	struct box *box = data;
	wl_fixed_t src_x, src_y, src_width, src_height;

	/* Cut the green border in half, take white border fully in,
	 * and black border fully out. The borders are 1px wide in buffer.
	 *
	 * The gl-renderer uses linear texture sampling, this means the
	 * top and left edges go to 100% green, bottom goes to 50% blue/black,
	 * right edge has thick white sliding to 50% red.
	 */
	src_x = wl_fixed_from_double((RECT_X + 0.5) / BUFFER_SCALE);
	src_y = wl_fixed_from_double((RECT_Y + 0.5) / BUFFER_SCALE);
	src_width = wl_fixed_from_double((RECT_W - 0.5) / BUFFER_SCALE);
	src_height = wl_fixed_from_double((RECT_H - 0.5) / BUFFER_SCALE);

	if (strcmp(interface, "wl_scaler") == 0) {
		box->scaler = display_bind(display, name,
					   &wl_scaler_interface, 1);

		box->viewport = wl_scaler_get_viewport(box->scaler,
			widget_get_wl_surface(box->widget));

		wl_viewport_set(box->viewport,
				src_x, src_y, src_width, src_height,
				SURFACE_WIDTH, SURFACE_HEIGHT); /* dst */
	}
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button, enum wl_pointer_button_state state, void *data)
{
	struct box *box = data;

	if (button != BTN_LEFT)
		return;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		window_move(box->window, input,
			    display_get_serial(box->display));
	}
}

static void
touch_down_handler(struct widget *widget, struct input *input,
		   uint32_t serial, uint32_t time, int32_t id,
		   float x, float y, void *data)
{
	struct box *box = data;
	window_move(box->window, input,
		    display_get_serial(box->display));
}

int
main(int argc, char *argv[])
{
	struct box box;
	struct display *d;
	struct timeval tv;

	d = display_create(&argc, argv);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	gettimeofday(&tv, NULL);
	srandom(tv.tv_usec);

	box.width = BUFFER_WIDTH / BUFFER_SCALE;
	box.height = BUFFER_HEIGHT / BUFFER_SCALE;
	box.display = d;
	box.window = window_create(d);
	box.widget = window_add_widget(box.window, &box);
	window_set_title(box.window, "Scaler Test Box");
	window_set_buffer_scale(box.window, BUFFER_SCALE);

	widget_set_resize_handler(box.widget, resize_handler);
	widget_set_redraw_handler(box.widget, redraw_handler);
	widget_set_button_handler(box.widget, button_handler);
	widget_set_default_cursor(box.widget, CURSOR_HAND1);
	widget_set_touch_down_handler(box.widget, touch_down_handler);

	window_schedule_resize(box.window, box.width, box.height);

	display_set_user_data(box.display, &box);
	display_set_global_handler(box.display, global_handler);

	display_run(d);

	window_destroy(box.window);
	return 0;
}

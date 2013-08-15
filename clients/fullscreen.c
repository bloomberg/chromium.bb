/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2012 Intel Corporation
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
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <cairo.h>

#include <linux/input.h>
#include <wayland-client.h>
#include "window.h"

struct fullscreen {
	struct display *display;
	struct window *window;
	struct widget *widget;
	int width, height;
	int fullscreen;
	float pointer_x, pointer_y;
	enum wl_shell_surface_fullscreen_method fullscreen_method;
};

static void
fullscreen_handler(struct window *window, void *data)
{
	struct fullscreen *fullscreen = data;

	fullscreen->fullscreen ^= 1;
	window_set_fullscreen(window, fullscreen->fullscreen);
}

static void
resize_handler(struct widget *widget, int width, int height, void *data)
{
	struct fullscreen *fullscreen = data;

	widget_set_size(widget, fullscreen->width, fullscreen->height);
}

static void
draw_string(cairo_t *cr,
	    const char *fmt, ...)
{
	char buffer[4096];
	char *p, *end;
	va_list argp;
	cairo_text_extents_t text_extents;
	cairo_font_extents_t font_extents;

	cairo_save(cr);

	cairo_select_font_face(cr, "sans",
			       CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 14);

	cairo_font_extents (cr, &font_extents);

	va_start(argp, fmt);

	vsnprintf(buffer, sizeof(buffer), fmt, argp);

	p = buffer;
	while (*p) {
		end = strchr(p, '\n');
		if (end)
			*end = 0;

		cairo_show_text(cr, p);
		cairo_text_extents (cr, p, &text_extents);
		cairo_rel_move_to (cr, -text_extents.x_advance, font_extents.height);

		if (end)
			p = end + 1;
		else
			break;
	}

	va_end(argp);

	cairo_restore(cr);

}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct fullscreen *fullscreen = data;
	struct rectangle allocation;
	cairo_surface_t *surface;
	cairo_t *cr;
	int i;
	double x, y, border;
	const char *method_name[] = { "default", "scale", "driver", "fill" };

	surface = window_get_surface(fullscreen->window);
	if (surface == NULL ||
	    cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to create cairo egl surface\n");
		return;
	}

	widget_get_allocation(fullscreen->widget, &allocation);

	cr = widget_cairo_create(widget);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_paint (cr);

	cairo_set_source_rgb(cr, 0, 0, 1);
	cairo_set_line_width (cr, 10);
	cairo_rectangle(cr, 5, 5, allocation.width - 10, allocation.height - 10);
	cairo_stroke (cr);

	cairo_move_to(cr,
		      allocation.x + 15,
		      allocation.y + 25);
	cairo_set_source_rgb(cr, 1, 1, 1);

	draw_string(cr,
		    "Surface size: %d, %d\n"
		    "Scale: %d, transform: %d\n"
		    "Pointer: %f,%f\n"
		    "Fullscreen: %d, method: %s\n"
		    "Keys: (s)cale, (t)ransform, si(z)e, (m)ethod, (f)ullscreen, (q)uit\n",
		    fullscreen->width, fullscreen->height,
		    window_get_buffer_scale (fullscreen->window),
		    window_get_buffer_transform (fullscreen->window),
		    fullscreen->pointer_x, fullscreen->pointer_y,
		    fullscreen->fullscreen, method_name[fullscreen->fullscreen_method]);

	y = 100;
	i = 0;
	while (y + 60 < fullscreen->height) {
		border = (i++ % 2 == 0) ? 1 : 0.5;

		x = 50;
		cairo_set_line_width (cr, border);
		while (x + 70 < fullscreen->width) {
			if (fullscreen->pointer_x >= x && fullscreen->pointer_x < x + 50 &&
			    fullscreen->pointer_y >= y && fullscreen->pointer_y < y + 40) {
				cairo_set_source_rgb(cr, 1, 0, 0);
				cairo_rectangle(cr,
						x, y,
						50, 40);
				cairo_fill(cr);
			}
			cairo_set_source_rgb(cr, 0, 1, 0);
			cairo_rectangle(cr,
					x + border/2.0, y + border/2.0,
					50, 40);
			cairo_stroke(cr);
			x += 60;
		}

		y += 50;
	}

	cairo_destroy(cr);
}

static void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym, enum wl_keyboard_key_state state,
	    void *data)
{
	struct fullscreen *fullscreen = data;
	int transform, scale;
	static int current_size = 0;
	int widths[] = { 640, 320, 800, 400 };
	int heights[] = { 480, 240, 600, 300 };

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	switch (sym) {
	case XKB_KEY_t:
		transform = window_get_buffer_transform (window);
		transform = (transform + 1) % 8;
		window_set_buffer_transform(window, transform);
		window_schedule_redraw(window);
		break;

	case XKB_KEY_s:
		scale = window_get_buffer_scale (window);
		if (scale == 1)
			scale = 2;
		else
			scale = 1;
		window_set_buffer_scale(window, scale);
		window_schedule_redraw(window);
		break;

	case XKB_KEY_z:
		current_size = (current_size + 1) % 4;
		fullscreen->width = widths[current_size];
		fullscreen->height = heights[current_size];
		window_schedule_resize(fullscreen->window,
				       fullscreen->width, fullscreen->height);
		break;

	case XKB_KEY_m:
		fullscreen->fullscreen_method = (fullscreen->fullscreen_method + 1) % 4;
		window_set_fullscreen_method(fullscreen->window,
					     fullscreen->fullscreen_method);
		window_schedule_redraw(window);
		break;

	case XKB_KEY_f:
		fullscreen->fullscreen ^= 1;
		window_set_fullscreen(window, fullscreen->fullscreen);
		break;

	case XKB_KEY_q:
		exit (0);
		break;
	}
}

static int
motion_handler(struct widget *widget,
	       struct input *input,
	       uint32_t time,
	       float x,
	       float y, void *data)
{
	struct fullscreen *fullscreen = data;

	fullscreen->pointer_x = x;
	fullscreen->pointer_y = y;

	widget_schedule_redraw(widget);
	return 0;
}


static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button, enum wl_pointer_button_state state, void *data)
{
	struct fullscreen *fullscreen = data;

	switch (button) {
	case BTN_LEFT:
		if (state == WL_POINTER_BUTTON_STATE_PRESSED)
			window_move(fullscreen->window, input,
				    display_get_serial(fullscreen->display));
		break;
	case BTN_RIGHT:
		if (state == WL_POINTER_BUTTON_STATE_PRESSED)
			window_show_frame_menu(fullscreen->window, input, time);
		break;
	}
}

static void
touch_handler(struct widget *widget, struct input *input, 
		   uint32_t serial, uint32_t time, int32_t id, 
		   float x, float y, void *data)
{
	struct fullscreen *fullscreen = data;
	window_touch_move(fullscreen->window, input, 
			  display_get_serial(fullscreen->display));
}

static void
usage(int error_code)
{
	fprintf(stderr, "Usage: fullscreen [OPTIONS]\n\n"
		"   -w <width>\tSet window width to <width>\n"
		"   -h <height>\tSet window height to <height>\n"
		"   --help\tShow this help text\n\n");

	exit(error_code);
}

int main(int argc, char *argv[])
{
	struct fullscreen fullscreen;
	struct display *d;
	int i;

	fullscreen.width = 640;
	fullscreen.height = 480;
	fullscreen.fullscreen = 0;
	fullscreen.fullscreen_method =
		WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-w") == 0) {
			if (++i >= argc)
				usage(EXIT_FAILURE);

			fullscreen.width = atol(argv[i]);
		} else if (strcmp(argv[i], "-h") == 0) {
			if (++i >= argc)
				usage(EXIT_FAILURE);

			fullscreen.height = atol(argv[i]);
		} else if (strcmp(argv[i], "--help") == 0)
			usage(EXIT_SUCCESS);
		else
			usage(EXIT_FAILURE);
	}

	d = display_create(&argc, argv);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	fullscreen.display = d;
	fullscreen.window = window_create(d);
	fullscreen.widget =
		window_add_widget(fullscreen.window, &fullscreen);

	window_set_title(fullscreen.window, "Fullscreen");
	window_set_fullscreen_method(fullscreen.window,
				     fullscreen.fullscreen_method);

	widget_set_transparent(fullscreen.widget, 0);
	widget_set_default_cursor(fullscreen.widget, CURSOR_LEFT_PTR);

	widget_set_resize_handler(fullscreen.widget, resize_handler);
	widget_set_redraw_handler(fullscreen.widget, redraw_handler);
	widget_set_button_handler(fullscreen.widget, button_handler);
	widget_set_motion_handler(fullscreen.widget, motion_handler);

	widget_set_touch_down_handler(fullscreen.widget, touch_handler);

	window_set_key_handler(fullscreen.window, key_handler);
	window_set_fullscreen_handler(fullscreen.window, fullscreen_handler);

	window_set_user_data(fullscreen.window, &fullscreen);
	/* Hack to set minimum allocation so we can shrink later */
	window_schedule_resize(fullscreen.window,
			       1, 1);
	window_schedule_resize(fullscreen.window,
			       fullscreen.width, fullscreen.height);

	display_run(d);

	return 0;
}

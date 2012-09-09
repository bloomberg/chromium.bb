/*
 * Copyright © 2012 Openismus GmbH
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/input.h>
#include <cairo.h>

#include "window.h"
#include "input-method-client-protocol.h"
#include "desktop-shell-client-protocol.h"

struct virtual_keyboard {
	struct input_panel *input_panel;
	struct input_method *input_method;
	struct input_method_context *context;
	struct display *display;
};

struct keyboard {
	struct virtual_keyboard *keyboard;
	struct window *window;
	struct widget *widget;
	int cx;
	int cy;
};

static void
redraw_handler(struct widget *widget, void *data)
{
	struct keyboard *keyboard = data;
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_t *cr;
	int cx, cy;
	int i;

	surface = window_get_surface(keyboard->window);
	widget_get_allocation(keyboard->widget, &allocation);

	cr = cairo_create(surface);
	cairo_rectangle(cr, allocation.x, allocation.y, allocation.width, allocation.height);
	cairo_clip(cr);

	cairo_translate(cr, allocation.x, allocation.y);

	cx = keyboard->cx;
	cy = keyboard->cy;

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
	cairo_rectangle(cr, 0, 0, 10 * cx, 5 * cy);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	for (i = 0; i <= 'Z' - '0'; ++i) {
		const int x = i % 10;
		const int y = i / 10;
		char text[] = { i + '0', '\0' };
		cairo_text_extents_t extents;
		int dx, dy;

		cairo_text_extents(cr, text, &extents);
		dx = extents.width;
		dy = extents.height;

		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_rectangle(cr, x * cx, y * cy, cx, cy);
		cairo_stroke(cr);

		cairo_move_to(cr, x * cx + 0.5 * (cx - dx), y * cy + 0.5 * (cy - dy));

		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_show_text(cr, text);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height, void *data)
{
	/* struct keyboard *keyboard = data; */
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button,
	       enum wl_pointer_button_state state, void *data)
{
	struct keyboard *keyboard = data;
	struct rectangle allocation;
	int32_t x, y;	
	char text[] = { '0', '\0' };

	if (state != WL_POINTER_BUTTON_STATE_PRESSED || button != BTN_LEFT) {
		return;
	}

	input_get_position(input, &x, &y);

	widget_get_allocation(keyboard->widget, &allocation);
	x -= allocation.x;
	y -= allocation.y;

	text[0] = y / keyboard->cy * 10 + x / keyboard->cx + '0';

	input_method_context_commit_string(keyboard->keyboard->context, text, -1);

	widget_schedule_redraw(widget);
}

static void
input_method_context_surrounding_text(void *data,
				      struct input_method_context *context,
				      const char *text,
				      uint32_t cursor,
				      uint32_t anchor)
{
	fprintf(stderr, "Surrounding text updated: %s\n", text);
}

static const struct input_method_context_listener input_method_context_listener = {
	input_method_context_surrounding_text,
};

static void
input_method_activate(void *data,
		      struct input_method *input_method,
		      struct input_method_context *context)
{
	struct virtual_keyboard *keyboard = data;

	if (keyboard->context)
		input_method_context_destroy(keyboard->context);

	keyboard->context = context;
	input_method_context_add_listener(context,
					  &input_method_context_listener,
					  keyboard);
}

static void
input_method_deactivate(void *data,
			struct input_method *input_method,
			struct input_method_context *context)
{
	struct virtual_keyboard *keyboard = data;

	if (!keyboard->context)
		return;

	input_method_context_destroy(keyboard->context);
	keyboard->context = NULL;
}

static const struct input_method_listener input_method_listener = {
	input_method_activate,
	input_method_deactivate
};

static void
global_handler(struct wl_display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	struct virtual_keyboard *keyboard = data;

	if (!strcmp(interface, "input_panel")) {
		keyboard->input_panel = wl_display_bind(display, id, &input_panel_interface);
	} else if (!strcmp(interface, "input_method")) {
		keyboard->input_method = wl_display_bind(display, id, &input_method_interface);
		input_method_add_listener(keyboard->input_method, &input_method_listener, keyboard);
	}
}

static void
keyboard_create(struct output *output, struct virtual_keyboard *virtual_keyboard)
{
	struct keyboard *keyboard;

	keyboard = malloc(sizeof *keyboard);
	memset(keyboard, 0, sizeof *keyboard);

	keyboard->keyboard = virtual_keyboard;
	keyboard->window = window_create_custom(virtual_keyboard->display);
	keyboard->widget = window_add_widget(keyboard->window, keyboard);

	window_set_title(keyboard->window, "Virtual keyboard");
	window_set_user_data(keyboard->window, keyboard);
	
	keyboard->cx = 40;
	keyboard->cy = 40;

	widget_set_redraw_handler(keyboard->widget, redraw_handler);
	widget_set_resize_handler(keyboard->widget, resize_handler);
	widget_set_button_handler(keyboard->widget, button_handler);

	window_schedule_resize(keyboard->window, keyboard->cx * 10, keyboard->cy * 5);

	input_panel_set_surface(virtual_keyboard->input_panel,
				window_get_wl_surface(keyboard->window),
				output_get_wl_output(output));
}

static void
handle_output_configure(struct output *output, void *data)
{
	struct virtual_keyboard *virtual_keyboard = data;

	/* skip existing outputs */
	if (output_get_user_data(output))
		return;

	output_set_user_data(output, virtual_keyboard);

	keyboard_create(output, virtual_keyboard);
}

int
main(int argc, char *argv[])
{
	struct virtual_keyboard virtual_keyboard;

	virtual_keyboard.display = display_create(argc, argv);
	if (virtual_keyboard.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	virtual_keyboard.context = NULL;

	wl_display_add_global_listener(display_get_display(virtual_keyboard.display),
				       global_handler, &virtual_keyboard);

	display_set_user_data(virtual_keyboard.display, &virtual_keyboard);
	display_set_output_configure_handler(virtual_keyboard.display, handle_output_configure);

	display_run(virtual_keyboard.display);

	return 0;
}

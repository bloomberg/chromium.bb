/*
 * Copyright © 2008 Kristian Høgsberg
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
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>
#include <glib.h>
#include <cairo-drm.h>

#include <linux/input.h>
#include "wayland-util.h"
#include "wayland-client.h"
#include "wayland-glib.h"
#include "../cairo-util.h"

#include "window.h"

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_output *output;
	struct wl_input_device *input_device;
	struct rectangle screen_allocation;
	cairo_device_t *device;
	int fd;
};

struct window {
	struct display *display;
	struct wl_surface *surface;
	const char *title;
	struct rectangle allocation, saved_allocation;
	int minimum_width, minimum_height;
	int margin;
	int drag_x, drag_y;
	int state;
	int fullscreen;
	struct wl_input_device *grab_device;
	struct wl_input_device *keyboard_device;
	uint32_t name;
	uint32_t modifiers;

	cairo_surface_t *cairo_surface, *pending_surface;
	int new_surface;

	window_resize_handler_t resize_handler;
	window_key_handler_t key_handler;
	window_keyboard_focus_handler_t keyboard_focus_handler;
	void *user_data;
};

static void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius)
{
	cairo_move_to(cr, x0, y0 + radius);
	cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, 3 * M_PI / 2);
	cairo_line_to(cr, x1 - radius, y0);
	cairo_arc(cr, x1 - radius, y0 + radius, radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to(cr, x1, y1 - radius);
	cairo_arc(cr, x1 - radius, y1 - radius, radius, 0, M_PI / 2);
	cairo_line_to(cr, x0 + radius, y1);
	cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI / 2, M_PI);
	cairo_close_path(cr);
}

static void
window_attach_surface(struct window *window)
{
	struct wl_visual *visual;

	if (window->pending_surface != NULL)
		return;

	window->pending_surface =
		cairo_surface_reference(window->cairo_surface);

	visual = wl_display_get_premultiplied_argb_visual(window->display->display);
	wl_surface_attach(window->surface,
			  cairo_drm_surface_get_name(window->cairo_surface),
			  window->allocation.width,
			  window->allocation.height,
			  cairo_drm_surface_get_stride(window->cairo_surface),
			  visual);

	wl_surface_map(window->surface,
		       window->allocation.x - window->margin,
		       window->allocation.y - window->margin,
		       window->allocation.width,
		       window->allocation.height);
}

void
window_commit(struct window *window, uint32_t key)
{
	if (window->new_surface) {
		window_attach_surface(window);
		window->new_surface = 0;
	}

	wl_compositor_commit(window->display->compositor, key);
}

static void
window_draw_decorations(struct window *window)
{
	cairo_t *cr;
	int border = 2, radius = 5;
	cairo_text_extents_t extents;
	cairo_pattern_t *gradient, *outline, *bright, *dim;
	int width, height;
	int shadow_dx = 4, shadow_dy = 4;

	window->cairo_surface =
		cairo_drm_surface_create(window->display->device,
					 CAIRO_CONTENT_COLOR_ALPHA,
					 window->allocation.width,
					 window->allocation.height);

	outline = cairo_pattern_create_rgb(0.1, 0.1, 0.1);
	bright = cairo_pattern_create_rgb(0.8, 0.8, 0.8);
	dim = cairo_pattern_create_rgb(0.4, 0.4, 0.4);

	cr = cairo_create(window->cairo_surface);

	width = window->allocation.width - window->margin * 2;
	height = window->allocation.height - window->margin * 2;

	cairo_translate(cr, window->margin + shadow_dx,
			window->margin + shadow_dy);
	cairo_set_line_width (cr, border);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
	rounded_rect(cr, -1, -1, width + 1, height + 1, radius);
	cairo_fill(cr);

#define SLOW_BUT_PWETTY_not_right_now
#ifdef SLOW_BUT_PWETTY
	/* FIXME: Aw, pretty drop shadows now have to fallback to sw.
	 * Ideally we should have convolution filters in cairo, but we
	 * can also fallback to compositing the shadow image a bunch
	 * of times according to the blur kernel. */
	{
		cairo_surface_t *map;

		map = cairo_drm_surface_map(window->cairo_surface);
		blur_surface(map, 32);
		cairo_drm_surface_unmap(window->cairo_surface, map);
	}
#endif

	cairo_translate(cr, -shadow_dx, -shadow_dy);
	if (window->keyboard_device) {
		rounded_rect(cr, 0, 0, width, height, radius);
		gradient = cairo_pattern_create_linear (0, 0, 0, 100);
		cairo_pattern_add_color_stop_rgb(gradient, 0, 0.6, 0.6, 0.6);
		cairo_pattern_add_color_stop_rgb(gradient, 1, 0.8, 0.8, 0.8);
		cairo_set_source(cr, gradient);
		cairo_fill(cr);
		cairo_pattern_destroy(gradient);
	} else {
		rounded_rect(cr, 0, 0, width, height, radius);
		cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, 1);
		cairo_fill(cr);
	}

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_move_to(cr, 10, 50);
	cairo_line_to(cr, width - 10, 50);
	cairo_line_to(cr, width - 10, height - 10);
	cairo_line_to(cr, 10, height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	cairo_move_to(cr, 11, 51);
	cairo_line_to(cr, width - 10, 51);
	cairo_line_to(cr, width - 10, height - 10);
	cairo_line_to(cr, 11, height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_font_size(cr, 14);
	cairo_text_extents(cr, window->title, &extents);
	cairo_move_to(cr, (width - extents.width) / 2, 10 - extents.y_bearing);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 4);
	cairo_text_path(cr, window->title);
	if (window->keyboard_device) {
		cairo_set_source_rgb(cr, 0.56, 0.56, 0.56);
		cairo_stroke_preserve(cr);
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_fill(cr);
	} else {
		cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
		cairo_fill(cr);
	}
	cairo_destroy(cr);
}

static void
window_draw_fullscreen(struct window *window)
{
	window->cairo_surface =
		cairo_drm_surface_create(window->display->device,
					 CAIRO_CONTENT_COLOR_ALPHA,
					 window->allocation.width,
					 window->allocation.height);
}

void
window_draw(struct window *window)
{
	if (window->cairo_surface != NULL)
		cairo_surface_destroy(window->cairo_surface);

	if (window->fullscreen)
		window_draw_fullscreen(window);
	else
		window_draw_decorations(window);

	window->new_surface = 1;
}

static void
window_handle_acknowledge(void *data,
			  struct wl_compositor *compositor,
			  uint32_t key, uint32_t frame)
{
	struct window *window = data;
	cairo_surface_t *pending;

	/* The acknowledge event means that the server
	 * processed our last commit request and we can now
	 * safely free the old window buffer if we resized and
	 * render the next frame into our back buffer.. */

	pending = window->pending_surface;
	window->pending_surface = NULL;
	if (pending != window->cairo_surface)
		window_attach_surface(window);
	cairo_surface_destroy(pending);
}

static void
window_handle_frame(void *data,
		    struct wl_compositor *compositor,
		    uint32_t frame, uint32_t timestamp)
{
}

static const struct wl_compositor_listener compositor_listener = {
	window_handle_acknowledge,
	window_handle_frame,
};

enum window_state {
	WINDOW_STABLE,
	WINDOW_MOVING,
	WINDOW_RESIZING_UPPER_LEFT,
	WINDOW_RESIZING_UPPER_RIGHT,
	WINDOW_RESIZING_LOWER_LEFT,
	WINDOW_RESIZING_LOWER_RIGHT
};

enum location {
	LOCATION_INTERIOR,
	LOCATION_UPPER_LEFT,
	LOCATION_UPPER_RIGHT,
	LOCATION_LOWER_LEFT,
	LOCATION_LOWER_RIGHT,
	LOCATION_OUTSIDE
};

static void
window_handle_motion(void *data, struct wl_input_device *input_device,
		     int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct window *window = data;

	switch (window->state) {
	case WINDOW_MOVING:
		if (window->fullscreen)
			break;
		if (window->grab_device != input_device)
			break;
		window->allocation.x = window->drag_x + x;
		window->allocation.y = window->drag_y + y;
		wl_surface_map(window->surface,
			       window->allocation.x - window->margin,
			       window->allocation.y - window->margin,
			       window->allocation.width,
			       window->allocation.height);
		wl_compositor_commit(window->display->compositor, 1);
		break;
	case WINDOW_RESIZING_LOWER_RIGHT:
		if (window->fullscreen)
			break;
		if (window->grab_device != input_device)
			break;
		window->allocation.width = window->drag_x + x;
		window->allocation.height = window->drag_y + y;

		if (window->resize_handler)
			(*window->resize_handler)(window,
						  window->user_data);

		break;
	}
}

static void window_handle_button(void *data, struct wl_input_device *input_device,
				 uint32_t button, uint32_t state,
				 int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct window *window = data;
	int32_t left = window->allocation.x;
	int32_t right = window->allocation.x +
		window->allocation.width - window->margin * 2;
	int32_t top = window->allocation.y;
	int32_t bottom = window->allocation.y +
		window->allocation.height - window->margin * 2;
	int grip_size = 16, location;
	
	if (right - grip_size <= x && x < right &&
	    bottom - grip_size <= y && y < bottom) {
		location = LOCATION_LOWER_RIGHT;
	} else if (left <= x && x < right && top <= y && y < bottom) {
		location = LOCATION_INTERIOR;
	} else {
		location = LOCATION_OUTSIDE;
	}

	if (button == BTN_LEFT && state == 1) {
		switch (location) {
		case LOCATION_INTERIOR:
			window->drag_x = window->allocation.x - x;
			window->drag_y = window->allocation.y - y;
			window->state = WINDOW_MOVING;
			window->grab_device = input_device;
			break;
		case LOCATION_LOWER_RIGHT:
			window->drag_x = window->allocation.width - x;
			window->drag_y = window->allocation.height - y;
			window->state = WINDOW_RESIZING_LOWER_RIGHT;
			window->grab_device = input_device;
			break;
		default:
			window->state = WINDOW_STABLE;
			break;
		}
	} else if (button == BTN_LEFT &&
		   state == 0 && window->grab_device == input_device) {
		window->state = WINDOW_STABLE;
	}
}


struct key {
	uint32_t code[4];
} evdev_keymap[] = {
	{ { 0, 0 } },		/* 0 */
	{ { 0x1b, 0x1b } },
	{ { '1', '!' } },
	{ { '2', '@' } },
	{ { '3', '#' } },
	{ { '4', '$' } },
	{ { '5', '%' } },
	{ { '6', '^' } },
	{ { '7', '&' } },
	{ { '8', '*' } },
	{ { '9', '(' } },
	{ { '0', ')' } },
	{ { '-', '_' } },
	{ { '=', '+' } },
	{ { '\b', '\b' } },
	{ { '\t', '\t' } },

	{ { 'q', 'Q', 0x11 } },		/* 16 */
	{ { 'w', 'W', 0x17 } },
	{ { 'e', 'E', 0x05 } },
	{ { 'r', 'R', 0x12 } },
	{ { 't', 'T', 0x14 } },
	{ { 'y', 'Y', 0x19 } },
	{ { 'u', 'U', 0x15 } },
	{ { 'i', 'I', 0x09 } },
	{ { 'o', 'O', 0x0f } },
	{ { 'p', 'P', 0x10 } },
	{ { '[', '{', 0x1b } },
	{ { ']', '}', 0x1d } },
	{ { '\n', '\n' } },
	{ { 0, 0 } },
	{ { 'a', 'A', 0x01} },
	{ { 's', 'S', 0x13 } },

	{ { 'd', 'D', 0x04 } },		/* 32 */
	{ { 'f', 'F', 0x06 } },
	{ { 'g', 'G', 0x07 } },
	{ { 'h', 'H', 0x08 } },
	{ { 'j', 'J', 0x0a } },
	{ { 'k', 'K', 0x0b } },
	{ { 'l', 'L', 0x0c } },
	{ { ';', ':' } },
	{ { '\'', '"' } },
	{ { '`', '~' } },
	{ { 0, 0 } },
	{ { '\\', '|', 0x1c } },
	{ { 'z', 'Z', 0x1a } },
	{ { 'x', 'X', 0x18 } },
	{ { 'c', 'C', 0x03 } },
	{ { 'v', 'V', 0x16 } },

	{ { 'b', 'B', 0x02 } },		/* 48 */
	{ { 'n', 'N', 0x0e } },
	{ { 'm', 'M', 0x0d } },
	{ { ',', '<' } },
	{ { '.', '>' } },
	{ { '/', '?' } },
	{ { 0, 0 } },
	{ { '*', '*' } },
	{ { 0, 0 } },
	{ { ' ', ' ' } },
	{ { 0, 0 } }

	/* 59 */
};

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

static void
window_update_modifiers(struct window *window, uint32_t key, uint32_t state)
{
	uint32_t mod = 0;

	switch (key) {
	case KEY_LEFTSHIFT:
	case KEY_RIGHTSHIFT:
		mod = WINDOW_MODIFIER_SHIFT;
		break;
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		mod = WINDOW_MODIFIER_CONTROL;
		break;
	case KEY_LEFTALT:
	case KEY_RIGHTALT:
		mod = WINDOW_MODIFIER_ALT;
		break;
	}

	if (state)
		window->modifiers |= mod;
	else
		window->modifiers &= ~mod;
}

static void
window_handle_key(void *data, struct wl_input_device *input_device,
		  uint32_t key, uint32_t state)
{
	struct window *window = data;
	uint32_t unicode = 0;

	if (window->keyboard_device != input_device)
		return;

	window_update_modifiers(window, key, state);

	if (key < ARRAY_LENGTH(evdev_keymap)) {
		if (window->modifiers & WINDOW_MODIFIER_CONTROL)
			unicode = evdev_keymap[key].code[2];
		else if (window->modifiers & WINDOW_MODIFIER_SHIFT)
			unicode = evdev_keymap[key].code[1];
		else
			unicode = evdev_keymap[key].code[0];
	}

	if (window->key_handler)
		(*window->key_handler)(window, key, unicode,
				       state, window->modifiers, window->user_data);
}

static void
window_handle_pointer_focus(void *data,
			    struct wl_input_device *input_device,
			    struct wl_surface *surface)
{
}

static void
window_handle_keyboard_focus(void *data,
			     struct wl_input_device *input_device,
			     struct wl_surface *surface,
			     struct wl_array *keys)
{
	struct window *window = data;
	uint32_t *k, *end;

	if (window->keyboard_device == input_device && surface != window->surface)
		window->keyboard_device = NULL;
	else if (window->keyboard_device == NULL && surface == window->surface)
		window->keyboard_device = input_device;
	else
		return;

	if (window->keyboard_device) {
		end = keys->data + keys->size;
		for (k = keys->data; k < end; k++)
			window_update_modifiers(window, *k, 1);
	} else {
		window->modifiers = 0;
	}

	if (window->keyboard_focus_handler)
		(*window->keyboard_focus_handler)(window,
						  window->keyboard_device,
						  window->user_data);
}

static const struct wl_input_device_listener input_device_listener = {
	window_handle_motion,
	window_handle_button,
	window_handle_key,
	window_handle_pointer_focus,
	window_handle_keyboard_focus,
};

void
window_get_child_rectangle(struct window *window,
			   struct rectangle *rectangle)
{
	if (window->fullscreen) {
		*rectangle = window->allocation;
	} else {
		rectangle->x = window->margin + 10;
		rectangle->y = window->margin + 50;
		rectangle->width = window->allocation.width - 20 - window->margin * 2;
		rectangle->height = window->allocation.height - 60 - window->margin * 2;
	}
}

void
window_set_child_size(struct window *window,
		      struct rectangle *rectangle)
{
	if (!window->fullscreen) {
		window->allocation.width = rectangle->width + 20 + window->margin * 2;
		window->allocation.height = rectangle->height + 60 + window->margin * 2;
	}
}

cairo_surface_t *
window_create_surface(struct window *window,
		      struct rectangle *rectangle)
{
	return cairo_drm_surface_create(window->display->device,
					CAIRO_CONTENT_COLOR_ALPHA,
					rectangle->width,
					rectangle->height);
}

void
window_copy(struct window *window,
	    struct rectangle *rectangle,
	    uint32_t name, uint32_t stride)
{
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_drm_surface_create_for_name (window->display->device,
						     name, CAIRO_FORMAT_ARGB32,
						     rectangle->width, rectangle->height,
						     stride);

	cr = cairo_create (window->cairo_surface);

	cairo_set_source_surface (cr,
				  surface,
				  rectangle->x, rectangle->y);

	cairo_paint (cr);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);
}

void
window_copy_surface(struct window *window,
		    struct rectangle *rectangle,
		    cairo_surface_t *surface)
{
	cairo_t *cr;

	cr = cairo_create (window->cairo_surface);

	cairo_set_source_surface (cr,
				  surface,
				  rectangle->x, rectangle->y);

	cairo_paint (cr);
	cairo_destroy (cr);
}

void
window_set_fullscreen(struct window *window, int fullscreen)
{
	window->fullscreen = fullscreen;
	if (window->fullscreen) {
		window->saved_allocation = window->allocation;
		window->allocation = window->display->screen_allocation;
	} else {
		window->allocation = window->saved_allocation;
	}
}

void
window_set_resize_handler(struct window *window,
			  window_resize_handler_t handler, void *data)
{
	window->resize_handler = handler;
	window->user_data = data;
}

void
window_set_key_handler(struct window *window,
		       window_key_handler_t handler, void *data)
{
	window->key_handler = handler;
	window->user_data = data;
}

void
window_set_keyboard_focus_handler(struct window *window,
				  window_keyboard_focus_handler_t handler, void *data)
{
	window->keyboard_focus_handler = handler;
	window->user_data = data;
}

struct window *
window_create(struct display *display, const char *title,
	      int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct window *window;

	window = malloc(sizeof *window);
	if (window == NULL)
		return NULL;

	memset(window, 0, sizeof *window);
	window->display = display;
	window->title = strdup(title);
	window->surface = wl_compositor_create_surface(display->compositor);
	window->allocation.x = x;
	window->allocation.y = y;
	window->allocation.width = width;
	window->allocation.height = height;
	window->saved_allocation = window->allocation;
	window->margin = 16;
	window->state = WINDOW_STABLE;

	wl_compositor_add_listener(display->compositor,
				   &compositor_listener, window);

	wl_input_device_add_listener(display->input_device,
				     &input_device_listener, window);

	return window;
}

static void
display_handle_geometry(void *data,
			struct wl_output *output,
			int32_t width, int32_t height)
{
	struct display *display = data;

	display->screen_allocation.x = 0;
	display->screen_allocation.y = 0;
	display->screen_allocation.width = width;
	display->screen_allocation.height = height;
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
};

static void
display_handle_global(struct wl_display *display,
		     struct wl_object *object, void *data)
{
	struct display *d = data;

	if (wl_object_implements(object, "compositor", 1)) { 
		d->compositor = (struct wl_compositor *) object;
	} else if (wl_object_implements(object, "output", 1)) {
		d->output = (struct wl_output *) object;
		wl_output_add_listener(d->output, &output_listener, d);
	} else if (wl_object_implements(object, "input_device", 1)) {
		d->input_device =(struct wl_input_device *) object;
	}
}

struct display *
display_create(struct wl_display *display, int fd)
{
	struct display *d;

	d = malloc(sizeof *d);
	if (d == NULL)
		return NULL;

	d->display = display;
	d->device = cairo_drm_device_get_for_fd(fd);
	if (d->device == NULL) {
		fprintf(stderr, "failed to get cairo drm device\n");
		return NULL;
	}

	/* Set up listener so we'll catch all events. */
	wl_display_add_global_listener(display,
				       display_handle_global, d);

	/* Process connection events. */
	wl_display_iterate(display, WL_DISPLAY_READABLE);

	return d;
}

struct wl_compositor *
display_get_compositor(struct display *display)
{
	return display->compositor;
}

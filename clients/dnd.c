/*
 * Copyright © 2010 Kristian Høgsberg
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
#include <sys/time.h>
#include <cairo.h>
#include <sys/epoll.h>

#include <wayland-client.h>

#include "window.h"
#include "cairo-util.h"

struct dnd {
	struct window *window;
	struct display *display;
	uint32_t key;
	struct item *items[16];
	struct widget *widget;
};

struct dnd_drag {
	cairo_surface_t *translucent;
	cairo_surface_t *opaque;
	int hotspot_x, hotspot_y;
	struct dnd *dnd;
	struct input *input;
	uint32_t time;
	struct item *item;
	int x_offset, y_offset;
	const char *mime_type;

	struct wl_data_source *data_source;
};

struct item {
	cairo_surface_t *surface;
	int seed;
	int x, y;
};

struct dnd_flower_message {
	int seed, x_offset, y_offset;
};


static const int item_width = 64;
static const int item_height = 64;
static const int item_padding = 16;

static struct item *
item_create(struct display *display, int x, int y, int seed)
{
	struct item *item;
	struct timeval tv;

	item = malloc(sizeof *item);
	if (item == NULL)
		return NULL;
	
	
	gettimeofday(&tv, NULL);
	item->seed = seed ? seed : tv.tv_usec;
	srandom(item->seed);
	
	const int petal_count = 3 + random() % 5;
	const double r1 = 20 + random() % 10;
	const double r2 = 5 + random() % 12;
	const double u = (10 + random() % 90) / 100.0;
	const double v = (random() % 90) / 100.0;

	cairo_t *cr;
	int i;
	double t, dt = 2 * M_PI / (petal_count * 2);
	double x1, y1, x2, y2, x3, y3;
	struct rectangle rect;


	rect.width = item_width;
	rect.height = item_height;
	item->surface = display_create_surface(display, NULL, &rect, 0);

	item->x = x;
	item->y = y;

	cr = cairo_create(item->surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_translate(cr, item_width / 2, item_height / 2);
	t = random();
	cairo_move_to(cr, cos(t) * r1, sin(t) * r1);
	for (i = 0; i < petal_count; i++, t += dt * 2) {
		x1 = cos(t) * r1;
		y1 = sin(t) * r1;
		x2 = cos(t + dt) * r2;
		y2 = sin(t + dt) * r2;
		x3 = cos(t + 2 * dt) * r1;
		y3 = sin(t + 2 * dt) * r1;

		cairo_curve_to(cr,
			       x1 - y1 * u, y1 + x1 * u,
			       x2 + y2 * v, y2 - x2 * v,
			       x2, y2);

		cairo_curve_to(cr,
			       x2 - y2 * v, y2 + x2 * v,
			       x3 + y3 * u, y3 - x3 * u,
			       x3, y3);
	}

	cairo_close_path(cr);

	cairo_set_source_rgba(cr,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 100) / 99.0);

	cairo_fill_preserve(cr);

	cairo_set_line_width(cr, 1);
	cairo_set_source_rgba(cr,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 50) / 49.0,
			      0.5 + (random() % 100) / 99.0);
	cairo_stroke(cr);

	cairo_destroy(cr);

	return item;
}

static void
dnd_draw(struct dnd *dnd)
{
	struct rectangle allocation;
	cairo_t *cr;
	cairo_surface_t *surface;
	int i;

	window_draw(dnd->window);

	surface = window_get_surface(dnd->window);
	cr = cairo_create(surface);
	window_get_child_allocation(dnd->window, &allocation);
	cairo_rectangle(cr, allocation.x, allocation.y,
			allocation.width, allocation.height);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
	cairo_fill(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	for (i = 0; i < ARRAY_LENGTH(dnd->items); i++) {
		if (!dnd->items[i])
			continue;
		cairo_set_source_surface(cr, dnd->items[i]->surface,
					 dnd->items[i]->x + allocation.x,
					 dnd->items[i]->y + allocation.y);
		cairo_paint(cr);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	window_flush(dnd->window);
}

static void
redraw_handler(struct window *window, void *data)
{
	struct dnd *dnd = data;

	dnd_draw(dnd);
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct dnd *dnd = data;

	window_schedule_redraw(dnd->window);
}

static int
dnd_add_item(struct dnd *dnd, struct item *item)
{
	int i;

	for (i = 0; i < ARRAY_LENGTH(dnd->items); i++) {
		if (dnd->items[i] == 0) {
			dnd->items[i] = item;
			return i;
		}
	}
	return -1;
}

static struct item *
dnd_get_item(struct dnd *dnd, int32_t x, int32_t y)
{
	struct item *item;
	struct rectangle allocation;
	int i;

	window_get_child_allocation(dnd->window, &allocation);

	x -= allocation.x;
	y -= allocation.y;

	for (i = 0; i < ARRAY_LENGTH(dnd->items); i++) {
		item = dnd->items[i];
		if (item &&
		    item->x <= x && x < item->x + item_width &&
		    item->y <= y && y < item->y + item_height)
			return item;
	}

	return NULL;
}

static void
data_source_target(void *data,
		   struct wl_data_source *source, const char *mime_type)
{
	struct dnd_drag *dnd_drag = data;
	struct dnd *dnd = dnd_drag->dnd;
	cairo_surface_t *surface;
	struct wl_buffer *buffer;
	struct wl_data_device *device;

	device = input_get_data_device(dnd_drag->input);
	dnd_drag->mime_type = mime_type;
	if (mime_type)
		surface = dnd_drag->opaque;
	else
		surface = dnd_drag->translucent;

	buffer = display_get_buffer_for_surface(dnd->display, surface);
	wl_data_device_attach(device, dnd_drag->time, buffer,
				  dnd_drag->hotspot_x, dnd_drag->hotspot_y);
}

static void
data_source_send(void *data, struct wl_data_source *source,
		 const char *mime_type, int32_t fd)
{
	struct dnd_flower_message dnd_flower_message;	
	struct dnd_drag *dnd_drag = data;
	
	dnd_flower_message.seed = dnd_drag->item->seed;
	dnd_flower_message.x_offset = dnd_drag->x_offset;
	dnd_flower_message.y_offset = dnd_drag->y_offset;

	write(fd, &dnd_flower_message, sizeof dnd_flower_message);
	close(fd);
}

static void
data_source_cancelled(void *data, struct wl_data_source *source)
{
	struct dnd_drag *dnd_drag = data;

	/* The 'cancelled' event means that the source is no longer in
	 * use by the drag (or current selection).  We need to clean
	 * up the drag object created and the local state. */

	wl_data_source_destroy(dnd_drag->data_source);
	
	/* Destroy the item that has been dragged out */
	cairo_surface_destroy(dnd_drag->item->surface);
	free(dnd_drag->item);
	
	cairo_surface_destroy(dnd_drag->translucent);
	cairo_surface_destroy(dnd_drag->opaque);
	free(dnd_drag);
}

static const struct wl_data_source_listener data_source_listener = {
	data_source_target,
	data_source_send,
	data_source_cancelled
};

static cairo_surface_t *
create_drag_cursor(struct dnd_drag *dnd_drag,
		   struct item *item, int32_t x, int32_t y, double opacity)
{
	struct dnd *dnd = dnd_drag->dnd;
	cairo_surface_t *surface, *pointer;
	int32_t pointer_width, pointer_height, hotspot_x, hotspot_y;
	struct rectangle rectangle;
	cairo_pattern_t *pattern;
	cairo_t *cr;

	pointer = display_get_pointer_surface(dnd->display,
					      POINTER_DRAGGING,
					      &pointer_width,
					      &pointer_height,
					      &hotspot_x,
					      &hotspot_y);

	rectangle.width = item_width + 2 * pointer_width;
	rectangle.height = item_height + 2 * pointer_height;
	surface = display_create_surface(dnd->display, NULL, &rectangle, 0);

	cr = cairo_create(surface);
	cairo_translate(cr, pointer_width, pointer_height);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_surface(cr, item->surface, 0, 0);
	pattern = cairo_pattern_create_rgba(0, 0, 0, opacity);
	cairo_mask(cr, pattern);
	cairo_pattern_destroy(pattern);

	cairo_set_source_surface(cr, pointer,
				 x - item->x - hotspot_x,
				 y - item->y - hotspot_y);
	cairo_surface_destroy(pointer);
	cairo_paint(cr);
	/* FIXME: more cairo-gl brokeness */
	surface_flush_device(surface);
	cairo_destroy(cr);

	dnd_drag->hotspot_x = pointer_width + x - item->x;
	dnd_drag->hotspot_y = pointer_height + y - item->y;

	return surface;
}

static void
dnd_button_handler(struct window *window,
		   struct input *input, uint32_t time,
		   int button, int state, void *data)
{
	struct dnd *dnd = data;
	int32_t x, y;
	struct item *item;
	struct rectangle allocation;
	struct dnd_drag *dnd_drag;
	int i;

	window_get_child_allocation(dnd->window, &allocation);
	input_get_position(input, &x, &y);
	item = dnd_get_item(dnd, x, y);
	x -= allocation.x;
	y -= allocation.y;

	if (item && state == 1) {
		dnd_drag = malloc(sizeof *dnd_drag);
		dnd_drag->dnd = dnd;
		dnd_drag->input = input;
		dnd_drag->time = time;
		dnd_drag->item = item;
		dnd_drag->x_offset = x - item->x;
		dnd_drag->y_offset = y - item->y;

		for (i = 0; i < ARRAY_LENGTH(dnd->items); i++) {
			if (item == dnd->items[i]){
				dnd->items[i] = 0;
				break;
			}
		}

		dnd_drag->data_source =
			display_create_data_source(dnd->display);
		wl_data_source_add_listener(dnd_drag->data_source,
					    &data_source_listener,
					    dnd_drag);
		wl_data_source_offer(dnd_drag->data_source,
				     "application/x-wayland-dnd-flower");
		wl_data_source_offer(dnd_drag->data_source,
				     "text/plain; charset=utf-8");
		wl_data_device_start_drag(input_get_data_device(input),
					  dnd_drag->data_source,
					  window_get_wl_surface(window),
					  time);

		input_set_pointer_image(input, time, POINTER_DRAGGING);

		dnd_drag->opaque =
			create_drag_cursor(dnd_drag, item, x, y, 1);
		dnd_drag->translucent =
			create_drag_cursor(dnd_drag, item, x, y, 0.2);

		window_schedule_redraw(dnd->window);
	}
}

static int
lookup_cursor(struct dnd *dnd, int x, int y)
{
	struct item *item;

	item = dnd_get_item(dnd, x, y);
	if (item)
		return POINTER_HAND1;
	else
		return POINTER_LEFT_PTR;
}

static void
dnd_enter_handler(struct widget *widget,
		  struct input *input, uint32_t time,
		  int32_t x, int32_t y, void *data)
{
}

static int
dnd_motion_handler(struct widget *widget,
		   struct input *input, uint32_t time,
		   int32_t x, int32_t y, void *data)
{
	return lookup_cursor(data, x, y);
}

static void
dnd_data_handler(struct window *window,
		 struct input *input, uint32_t time,
		 int32_t x, int32_t y, const char **types, void *data)
{
	struct dnd *dnd = data;

	if (!dnd_get_item(dnd, x, y)) {
		input_accept(input, time, types[0]);
	} else {
		input_accept(input, time, NULL);
	}
}

static void
dnd_receive_func(void *data, size_t len, int32_t x, int32_t y, void *user_data)
{
	struct dnd *dnd = user_data;
	struct dnd_flower_message *message = data;
	struct item *item;
	struct rectangle allocation;

	if (len == 0) {
		return;
	} else if (len != sizeof *message) {
		fprintf(stderr, "odd message length %ld, expected %ld\n",
			len, sizeof *message);
		return;
	}
		
	window_get_child_allocation(dnd->window, &allocation);
	item = item_create(dnd->display,
			   x - message->x_offset - allocation.x,
			   y - message->y_offset - allocation.y,
			   message->seed);

	dnd_add_item(dnd, item);
	window_schedule_redraw(dnd->window);
}

static void
dnd_drop_handler(struct window *window, struct input *input,
		 int32_t x, int32_t y, void *data)
{
	struct dnd *dnd = data;

	if (dnd_get_item(dnd, x, y)) {
		fprintf(stderr, "got 'drop', but no target\n");
		return;
	}

	input_receive_drag_data(input, 
				"application/x-wayland-dnd-flower",
				dnd_receive_func, dnd);
}

static struct dnd *
dnd_create(struct display *display)
{
	struct dnd *dnd;
	int i, x, y;
	int32_t width, height;

	dnd = malloc(sizeof *dnd);
	if (dnd == NULL)
		return dnd;
	memset(dnd, 0, sizeof *dnd);

	dnd->window = window_create(display, 400, 400);
	window_set_title(dnd->window, "Wayland Drag and Drop Demo");

	dnd->display = display;
	dnd->key = 100;

	for (i = 0; i < ARRAY_LENGTH(dnd->items); i++) {
		x = (i % 4) * (item_width + item_padding) + item_padding;
		y = (i / 4) * (item_height + item_padding) + item_padding;
		if ((i ^ (i >> 2)) & 1)
			dnd->items[i] = item_create(display, x, y, 0);
		else
			dnd->items[i] = NULL;
	}

	window_set_user_data(dnd->window, dnd);
	window_set_redraw_handler(dnd->window, redraw_handler);
	window_set_keyboard_focus_handler(dnd->window,
					  keyboard_focus_handler);
	window_set_button_handler(dnd->window, dnd_button_handler);
	window_set_data_handler(dnd->window, dnd_data_handler);
	window_set_drop_handler(dnd->window, dnd_drop_handler);

	dnd->widget = window_add_widget(dnd->window, dnd);
	widget_set_enter_handler(dnd->widget, dnd_enter_handler);
	widget_set_motion_handler(dnd->widget, dnd_motion_handler);

	width = 4 * (item_width + item_padding) + item_padding;
	height = 4 * (item_height + item_padding) + item_padding;
	window_set_child_size(dnd->window, width, height);

	dnd_draw(dnd);

	return dnd;
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

	dnd_create(d);

	display_run(d);

	return 0;
}

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
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "wayland-client.h"
#include "wayland-glib.h"

#include "window.h"

static const char socket_name[] = "\0wayland";

struct dnd {
	struct window *window;
	struct display *display;
	uint32_t key;
	struct item *items[16];

	struct wl_buffer *translucent_buffer;
	struct wl_buffer *opaque_buffer;
	int hotspot_x, hotspot_y;
	uint32_t tag;
	const char *drag_type;
};

struct item {
	cairo_surface_t *surface;
	int x, y;
};

static const int item_width = 64;
static const int item_height = 64;
static const int item_padding = 16;

static struct item *
item_create(struct display *display, int x, int y)
{
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
	struct item *item;

	item = malloc(sizeof *item);
	if (item == NULL)
		return NULL;

	rect.width = item_width;
	rect.height = item_height;
	item->surface = display_create_surface(display, &rect);

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
	struct rectangle rectangle;
	cairo_t *cr;
	cairo_surface_t *wsurface, *surface;
	int i;

	window_draw(dnd->window);

	window_get_child_rectangle(dnd->window, &rectangle);

	wsurface = window_get_surface(dnd->window);
	surface = cairo_surface_create_similar(wsurface,
					       CAIRO_CONTENT_COLOR_ALPHA,
					       rectangle.width,
					       rectangle.height);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	for (i = 0; i < ARRAY_LENGTH(dnd->items); i++) {
		if (!dnd->items[i])
			continue;
		cairo_set_source_surface(cr, dnd->items[i]->surface,
					 dnd->items[i]->x, dnd->items[i]->y);
		cairo_paint(cr);
	}

	cairo_destroy(cr);

	window_copy_surface(dnd->window, &rectangle, surface);
	window_commit(dnd->window, dnd->key);
	cairo_surface_destroy(surface);
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

static struct item *
dnd_get_item(struct dnd *dnd, int32_t x, int32_t y)
{
	struct item *item;
	struct rectangle rectangle;
	int i;

	window_get_child_rectangle(dnd->window, &rectangle);

	x -= rectangle.x;
	y -= rectangle.y;

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
drag_handle_device(void *data,
		   struct wl_drag *drag, struct wl_input_device *device)
{
}

static void
drag_pointer_focus(void *data,
		   struct wl_drag *drag,
		   uint32_t time, struct wl_surface *surface,
		   int32_t x, int32_t y, int32_t surface_x, int32_t surface_y)
{
	struct dnd *dnd = data;

	/* FIXME: We need the offered types before we get the
	 * pointer_focus event so we know which one we want and can
	 * send the accept request back. */

	fprintf(stderr, "drag pointer focus %p\n", surface);

	if (!surface) {
		dnd->drag_type = NULL;
		return;
	}

	if (!dnd_get_item(dnd, surface_x, surface_y)) {
		wl_drag_accept(drag, "text/plain");
		dnd->drag_type = "text/plain";
	} else {
		wl_drag_accept(drag, NULL);
		dnd->drag_type = NULL;
	}
}

static void
drag_offer(void *data,
	   struct wl_drag *drag, const char *type)
{
	fprintf(stderr, "drag offer %s\n", type);
}

static void
drag_motion(void *data,
	    struct wl_drag *drag,
	    uint32_t time,
	    int32_t x, int32_t y, int32_t surface_x, int32_t surface_y)
{
	struct dnd *dnd = data;

	/* FIXME: Need to correlate this with the offer event.
	 * Problem is, we don't know when we've seen that last offer
	 * event, and we might need to look at all of them before we
	 * can decide which one to go with. */
	if (!dnd_get_item(dnd, surface_x, surface_y)) {
		wl_drag_accept(drag, "text/plain");
		dnd->drag_type = "text/plain";
	} else {
		wl_drag_accept(drag, NULL);
		dnd->drag_type = NULL;
	}
}

static void
drag_target(void *data,
	    struct wl_drag *drag, const char *mime_type)
{
	struct dnd *dnd = data;
	struct input *input;
	struct wl_input_device *device;

	fprintf(stderr, "target %s\n", mime_type);
	input = wl_drag_get_user_data(drag);
	device = input_get_input_device(input);
	if (mime_type)
		wl_input_device_attach(device, dnd->opaque_buffer,
				       dnd->hotspot_x, dnd->hotspot_y);
	else
		wl_input_device_attach(device, dnd->translucent_buffer,
				       dnd->hotspot_x, dnd->hotspot_y);
}

static gboolean
drop_io_func(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct dnd *dnd = data;
	char buffer[256];
	int fd;
	unsigned int len;
	GError *err = NULL;

	g_io_channel_read_chars(source, buffer, sizeof buffer, &len, &err);
	fprintf(stderr, "read %d bytes: %s\n", len, buffer);
	fd = g_io_channel_unix_get_fd(source);
	close(fd);
	g_source_remove(dnd->tag);

	g_io_channel_unref(source);

	return TRUE;
}

static void
drag_drop(void *data, struct wl_drag *drag)
{
	struct dnd *dnd = data;
	int p[2];
	GIOChannel *channel;

	if (!dnd->drag_type) {
		fprintf(stderr, "got 'drop', but no target\n");
		/* FIXME: Should send response so compositor and
		 * source knows it's over. Can't send -1 to indicate
		 * 'no target' though becauses of the way fd passing
		 * currently works.  */
		return;
	}

	fprintf(stderr, "got 'drop', sending write end of pipe\n");

	pipe(p);
	wl_drag_receive(drag, p[1]);
	close(p[1]);

	channel = g_io_channel_unix_new(p[0]);
	dnd->tag = g_io_add_watch(channel, G_IO_IN, drop_io_func, dnd);
}

static void
drag_finish(void *data, struct wl_drag *drag, int fd)
{
	char text[] = "[drop data]";

	fprintf(stderr, "got 'finish', fd %d, sending message\n", fd);

	write(fd, text, sizeof text);
	close(fd);
}

static const struct wl_drag_listener drag_listener = {
	drag_handle_device,
	drag_pointer_focus,
	drag_offer,
	drag_motion,
	drag_target,
	drag_drop,
	drag_finish
};

static cairo_surface_t *
create_drag_cursor(struct dnd *dnd, struct item *item, int32_t x, int32_t y,
		   double opacity)
{
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
	surface = display_create_surface(dnd->display, &rectangle);

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
	display_flush_cairo_device(dnd->display);
	cairo_destroy(cr);

	dnd->hotspot_x = pointer_width + x - item->x;
	dnd->hotspot_y = pointer_height + y - item->y;

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
	cairo_surface_t *surface;
	struct rectangle rectangle;

	window_get_child_rectangle(dnd->window, &rectangle);
	input_get_position(input, &x, &y);
	item = dnd_get_item(dnd, x, y);
	x -= rectangle.x;
	y -= rectangle.y;

	if (item && state == 1) {
		fprintf(stderr, "start drag, item %p\n", item);

		surface = create_drag_cursor(dnd, item, x, y, 1);
		dnd->opaque_buffer =
			display_get_buffer_for_surface(dnd->display, surface);

		surface = create_drag_cursor(dnd, item, x, y, 0.2);
		dnd->translucent_buffer =
			display_get_buffer_for_surface(dnd->display, surface);

		window_start_drag(window, input, time);

		/* FIXME: We leak the surface because we can't free it
		 * until the server has referenced it. */
	}
}

static int
dnd_motion_handler(struct window *window,
		   struct input *input, uint32_t time,
		   int32_t x, int32_t y,
		   int32_t sx, int32_t sy, void *data)
{
	struct dnd *dnd = data;
	struct item *item;

	item = dnd_get_item(dnd, sx, sy);

	if (item)
		return POINTER_HAND1;
	else
		return POINTER_LEFT_PTR;
}

static struct dnd *
dnd_create(struct display *display)
{
	struct dnd *dnd;
	gchar *title;
	int i, x, y;
	struct rectangle rectangle;

	dnd = malloc(sizeof *dnd);
	if (dnd == NULL)
		return dnd;
	memset(dnd, 0, sizeof *dnd);

	title = g_strdup_printf("Wayland Drag and Drop Demo");

	dnd->window = window_create(display, title, 100, 100, 500, 400);
	dnd->display = display;
	dnd->key = 100;

	for (i = 0; i < ARRAY_LENGTH(dnd->items); i++) {
		x = (i % 4) * (item_width + item_padding) + item_padding;
		y = (i / 4) * (item_height + item_padding) + item_padding;
		if ((i ^ (i >> 2)) & 1)
			dnd->items[i] = item_create(display, x, y);
		else
			dnd->items[i] = NULL;
	}

	window_set_user_data(dnd->window, dnd);
	window_set_redraw_handler(dnd->window, redraw_handler);
	window_set_keyboard_focus_handler(dnd->window,
					  keyboard_focus_handler);
	window_set_button_handler(dnd->window,
				  dnd_button_handler);

	window_set_motion_handler(dnd->window,
				  dnd_motion_handler);

	rectangle.width = 4 * (item_width + item_padding) + item_padding;
	rectangle.height = 4 * (item_height + item_padding) + item_padding;
	window_set_child_size(dnd->window, &rectangle);

	display_add_drag_listener(display, &drag_listener, dnd);

	dnd_draw(dnd);

	return dnd;
}

static const GOptionEntry option_entries[] = {
	{ NULL }
};

int
main(int argc, char *argv[])
{
	struct display *d;
	struct dnd *dnd;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	srandom(tv.tv_usec);

	d = display_create(&argc, &argv, option_entries);

	dnd = dnd_create (d);

	display_run(d);

	return 0;
}

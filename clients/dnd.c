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
};

struct dnd_drag {
	cairo_surface_t *translucent;
	cairo_surface_t *opaque;
	int hotspot_x, hotspot_y;
	struct dnd *dnd;
	struct input *input;
	uint32_t time;
};

struct dnd_offer {
	struct dnd *dnd;
	struct wl_array types;
	const char *drag_type;
	uint32_t tag;
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
	cairo_surface_destroy(wsurface);

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
drag_target(void *data,
	    struct wl_drag *drag, const char *mime_type)
{
	struct dnd_drag *dnd_drag = data;
	struct dnd *dnd = dnd_drag->dnd;
	struct wl_input_device *device;
	cairo_surface_t *surface;
	struct wl_buffer *buffer;

	fprintf(stderr, "target %s\n", mime_type);
	device = input_get_input_device(dnd_drag->input);
	if (mime_type)
		surface = dnd_drag->opaque;
	else
		surface = dnd_drag->translucent;

	buffer = display_get_buffer_for_surface(dnd->display, surface);
	wl_input_device_attach(device, dnd_drag->time, buffer,
			       dnd_drag->hotspot_x, dnd_drag->hotspot_y);
}

static void
drag_finish(void *data, struct wl_drag *drag, int fd)
{
	struct dnd_drag *dnd_drag = data;
	char text[] = "[drop data]";

	fprintf(stderr, "got 'finish', fd %d, sending message\n", fd);

	write(fd, text, sizeof text);
	close(fd);

	/* The 'finish' event marks the end of the session on the drag
	 * source side and we need to clean up the drag object created
	 * and the local state. */
	wl_drag_destroy(drag);
	cairo_surface_destroy(dnd_drag->translucent);
	cairo_surface_destroy(dnd_drag->opaque);
	free(dnd_drag);
}

static void
drag_reject(void *data, struct wl_drag *drag)
{
	fprintf(stderr, "drop rejected\n");
}

static const struct wl_drag_listener drag_listener = {
	drag_target,
	drag_finish,
	drag_reject
};

static void
drag_offer_offer(void *data,
		 struct wl_drag_offer *offer, const char *type)
{
	struct dnd_offer *dnd_offer = data;
	char **p;

	p = wl_array_add(&dnd_offer->types, sizeof *p);
	if (p)
		*p = strdup(type);
}

static void
drag_offer_pointer_focus(void *data,
			 struct wl_drag_offer *offer,
			 uint32_t time, struct wl_surface *surface,
			 int32_t x, int32_t y,
			 int32_t surface_x, int32_t surface_y)
{
	struct dnd_offer *dnd_offer = data;
	struct window *window;
	char **p, **end;

	/* The last event in a dnd session is pointer_focus with a
	 * NULL surface, whether or not we get the drop event.  We
	 * need to clean up the dnd_offer proxy and whatever state we
	 * allocated. */
	if (!surface) {
		fprintf(stderr, "pointer focus NULL, session over\n");
		wl_array_release(&dnd_offer->types);
		free(dnd_offer);
		wl_drag_offer_destroy(offer);
		return;
	}

	fprintf(stderr, "drag pointer focus %p\n", surface);
	fprintf(stderr, "offered types:\n");
	end = dnd_offer->types.data + dnd_offer->types.size;
	for (p = dnd_offer->types.data; p < end; p++)
		fprintf(stderr, "\%s\n", *p);

	window = wl_surface_get_user_data(surface);
	dnd_offer->dnd = window_get_user_data(window);

	if (!dnd_get_item(dnd_offer->dnd, surface_x, surface_y)) {
		wl_drag_offer_accept(offer, time, "text/plain");
		dnd_offer->drag_type = "text/plain";
	} else {
		wl_drag_offer_accept(offer, time, NULL);
		dnd_offer->drag_type = NULL;
	}
}

static void
drag_offer_motion(void *data,
		  struct wl_drag_offer *offer, uint32_t time,
		  int32_t x, int32_t y, int32_t surface_x, int32_t surface_y)
{
	struct dnd_offer *dnd_offer = data;
	struct dnd *dnd = dnd_offer->dnd;

	if (!dnd_get_item(dnd, surface_x, surface_y)) {
		fprintf(stderr, "drag offer motion %d, %d, accepting\n",
			surface_x, surface_y);
		wl_drag_offer_accept(offer, time, "text/plain");
		dnd_offer->drag_type = "text/plain";
	} else {
		fprintf(stderr, "drag offer motion %d, %d, declining\n",
			surface_x, surface_y);
		wl_drag_offer_accept(offer, time, NULL);
		dnd_offer->drag_type = NULL;
	}
}

static gboolean
drop_io_func(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct dnd_offer *dnd_offer = data;
	char buffer[256];
	int fd;
	unsigned int len;
	GError *err = NULL;

	g_io_channel_read_chars(source, buffer, sizeof buffer, &len, &err);
	fprintf(stderr, "read %d bytes: %s\n", len, buffer);
	fd = g_io_channel_unix_get_fd(source);
	close(fd);
	g_source_remove(dnd_offer->tag);

	g_io_channel_unref(source);

	return TRUE;
}

static void
drag_offer_drop(void *data, struct wl_drag_offer *offer)
{
	struct dnd_offer *dnd_offer = data;
	GIOChannel *channel;
	int p[2];

	if (!dnd_offer->drag_type) {
		fprintf(stderr, "got 'drop', but no target\n");
		wl_drag_offer_reject(offer);
		return;
	}

	fprintf(stderr, "got 'drop', sending write end of pipe\n");

	pipe(p);
	wl_drag_offer_receive(offer, p[1]);
	close(p[1]);

	channel = g_io_channel_unix_new(p[0]);
	dnd_offer->tag = g_io_add_watch(channel, G_IO_IN,
					drop_io_func, dnd_offer);
}

static const struct wl_drag_offer_listener drag_offer_listener = {
	drag_offer_offer,
	drag_offer_pointer_focus,
	drag_offer_motion,
	drag_offer_drop,
};

static void
drag_offer_handler(struct wl_drag_offer *offer, struct display *display)
{
	struct dnd_offer *dnd_offer;

	dnd_offer = malloc(sizeof *dnd_offer);
	if (dnd_offer == NULL)
		return;

	wl_drag_offer_add_listener(offer, &drag_offer_listener, dnd_offer);
	wl_array_init(&dnd_offer->types);
}

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
	struct rectangle rectangle;
	struct dnd_drag *dnd_drag;
	struct wl_drag *drag;

	window_get_child_rectangle(dnd->window, &rectangle);
	input_get_position(input, &x, &y);
	item = dnd_get_item(dnd, x, y);
	x -= rectangle.x;
	y -= rectangle.y;

	if (item && state == 1) {
		fprintf(stderr, "start drag, item %p\n", item);

		dnd_drag = malloc(sizeof *dnd_drag);
		dnd_drag->dnd = dnd;
		dnd_drag->input = input;
		dnd_drag->time = time;

		dnd_drag->opaque =
			create_drag_cursor(dnd_drag, item, x, y, 1);
		dnd_drag->translucent =
			create_drag_cursor(dnd_drag, item, x, y, 0.2);

		drag = window_create_drag(window);
		wl_drag_offer(drag, "text/plain");
		wl_drag_offer(drag, "text/html");
		window_activate_drag(drag, window, input, time);
		wl_drag_add_listener(drag, &drag_listener, dnd_drag);
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
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	display_set_drag_offer_handler(d, drag_offer_handler);

	dnd = dnd_create (d);

	display_run(d);

	return 0;
}

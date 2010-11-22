/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2009 Chris Wilson
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
#include <linux/input.h>

#include <glib/poppler-document.h>
#include <glib/poppler-page.h>

#include "wayland-util.h"
#include "wayland-client.h"
#include "wayland-glib.h"

#include "window.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

struct view {
	struct window *window;
	struct display *display;
	uint32_t key;

	gchar *filename;
	PopplerDocument *document;
	int page;
	int fullscreen;
	int focused;
};

static void
view_draw(struct view *view)
{
	struct rectangle rectangle;
	cairo_surface_t *surface;
	cairo_t *cr;
	PopplerPage *page;
	double width, height, doc_aspect, window_aspect, scale;

	window_draw(view->window);

	window_get_child_rectangle(view->window, &rectangle);

	page = poppler_document_get_page(view->document, view->page);

	surface = window_get_surface(view->window);

	cr = cairo_create(surface);
	cairo_rectangle(cr, rectangle.x, rectangle.y,
			 rectangle.width, rectangle.height);
	cairo_clip(cr);

	cairo_set_source_rgba(cr, 0, 0, 0, 0.8); 
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	poppler_page_get_size(page, &width, &height);
	doc_aspect = width / height;
	window_aspect = (double) rectangle.width / rectangle.height;
	if (doc_aspect < window_aspect)
		scale = rectangle.height / height;
	else
		scale = rectangle.width / width;
	cairo_translate(cr, rectangle.x, rectangle.y);
	cairo_scale(cr, scale, scale);
	cairo_translate(cr,
			(rectangle.width - width * scale) / 2 / scale,
			(rectangle.height - height * scale) / 2 / scale);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	poppler_page_render(page, cr);
	cairo_destroy(cr);
	g_object_unref(G_OBJECT(page));
	window_flush(view->window);
}

static void
redraw_handler(struct window *window, void *data)
{
	struct view *view = data;

	view_draw(view);
}

static void
key_handler(struct window *window, uint32_t key, uint32_t unicode,
	    uint32_t state, uint32_t modifiers, void *data)
{
	struct view *view = data;

	switch (key) {
	case KEY_F11:
		if (!state)
			break;
		view->fullscreen ^= 1;
		window_set_fullscreen(window, view->fullscreen);
		window_schedule_redraw(view->window);
		break;
	case KEY_SPACE:
	case KEY_PAGEDOWN:
		if (!state)
			break;
		view->page++;
		window_schedule_redraw(view->window);
		break;
	case KEY_BACKSPACE:
	case KEY_PAGEUP:
		if (!state)
			break;
		view->page--;
		window_schedule_redraw(view->window);
		break;
	default:
		break;
	}
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct view *view = data;

	view->focused = (device != NULL);
	window_schedule_redraw(view->window);
}

static struct view *
view_create(struct display *display, uint32_t key, const char *filename)
{
	struct view *view;
	gchar *basename;
	gchar *title;
	GError *error = NULL;

	view = malloc(sizeof *view);
	if (view == NULL)
		return view;
	memset(view, 0, sizeof *view);

	basename = g_path_get_basename(filename);
	title = g_strdup_printf("Wayland View - %s", basename);
	g_free(basename);

	view->filename = g_strdup(filename);

	view->window = window_create(display, title,
				     100 * key, 100 * key, 500, 400);
	view->display = display;

	/* FIXME: Window uses key 1 for moves, need some kind of
	 * allocation scheme here.  Or maybe just a real toolkit. */
	view->key = key + 100;

	window_set_user_data(view->window, view);
	window_set_redraw_handler(view->window, redraw_handler);
	window_set_key_handler(view->window, key_handler);
	window_set_keyboard_focus_handler(view->window,
					  keyboard_focus_handler);

	view->document = poppler_document_new_from_file(view->filename,
							NULL, &error);
	view->page = 0;
	view_draw(view);

	return view;
}

static const GOptionEntry option_entries[] = {
	{ NULL }
};

int
main(int argc, char *argv[])
{
	struct display *d;
	int i;

	d = display_create(&argc, &argv, option_entries);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	for (i = 1; i < argc; i++) {
		struct view *view;

		view = view_create (d, i, argv[i]);
	}

	display_run(d);

	return 0;
}

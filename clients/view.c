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

struct view {
	struct window *window;
	struct display *display;

	PopplerDocument *document;
	int page;
	int fullscreen;
};

static void
view_draw(struct view *view)
{
	struct rectangle allocation;
	cairo_surface_t *surface;
	cairo_t *cr;
	PopplerPage *page;
	double width, height, doc_aspect, window_aspect, scale;

	if (view->fullscreen)
		window_set_transparent(view->window, 0);
	else
		window_set_transparent(view->window, 1);

	window_draw(view->window);

	window_get_child_allocation(view->window, &allocation);

	surface = window_get_surface(view->window);

	cr = cairo_create(surface);
	cairo_rectangle(cr, allocation.x, allocation.y,
			 allocation.width, allocation.height);
	cairo_clip(cr);

	cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);

        if(!view->document) {
                cairo_destroy(cr);
                cairo_surface_destroy(surface);
                window_flush(view->window);
                return;
        }

	page = poppler_document_get_page(view->document, view->page);
	poppler_page_get_size(page, &width, &height);
	doc_aspect = width / height;
	window_aspect = (double) allocation.width / allocation.height;
	if (doc_aspect < window_aspect)
		scale = allocation.height / height;
	else
		scale = allocation.width / width;
	cairo_translate(cr, allocation.x, allocation.y);
	cairo_scale(cr, scale, scale);
	cairo_translate(cr,
			(allocation.width - width * scale) / 2 / scale,
			(allocation.height - height * scale) / 2 / scale);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	poppler_page_render(page, cr);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
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
resize_handler(struct window *window,
	       int32_t width, int32_t height, void *data)
{
	struct view *view = data;

	window_set_child_size(view->window, width, height);
	window_schedule_redraw(window);
}

static void
view_page_up(struct view *view)
{
        if(view->page <= 0)
                return;

        view->page--;
        window_schedule_redraw(view->window);
}

static void
view_page_down(struct view *view)
{
        if(!view->document ||
           view->page >= poppler_document_get_n_pages(view->document) - 1) {
                return;
        }

        view->page++;
        window_schedule_redraw(view->window);
}

static void
button_handler(struct window *window, struct input *input, uint32_t time,
               int button, int state, void *data)
{
        struct view *view = data;

        if(!state)
                return;

        switch(button) {
        case 275:
                view_page_up(view);
                break;
        case 276:
                view_page_down(view);
                break;
        default:
                break;
        }
}

static void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t unicode, uint32_t state, void *data)
{
	struct view *view = data;

	if(!state)
	        return;

	switch (key) {
	case KEY_F11:
		view->fullscreen ^= 1;
		window_set_fullscreen(window, view->fullscreen);
		window_schedule_redraw(view->window);
		break;
	case KEY_SPACE:
	case KEY_PAGEDOWN:
	case KEY_RIGHT:
	case KEY_DOWN:
                view_page_down(view);
		break;
	case KEY_BACKSPACE:
	case KEY_PAGEUP:
	case KEY_LEFT:
	case KEY_UP:
                view_page_up(view);
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
	window_schedule_redraw(view->window);
}

static struct view *
view_create(struct display *display,
	    uint32_t key, const char *filename, int fullscreen)
{
	struct view *view;
	gchar *basename;
	gchar *title;
	GFile *file = NULL;
	GError *error = NULL;

	view = malloc(sizeof *view);
	if (view == NULL)
		return view;
	memset(view, 0, sizeof *view);

	file = g_file_new_for_commandline_arg(filename);
	basename = g_file_get_basename(file);
	if(!basename) {
	        title = "Wayland View";
	} else {
	        title = g_strdup_printf("Wayland View - %s", basename);
	        g_free(basename);
	}

        view->document = poppler_document_new_from_file(g_file_get_uri(file),
                                                        NULL, &error);

        if(error) {
                title = "File not found";
        }

	view->window = window_create(display, 500, 400);
	window_set_title(view->window, title);
	view->display = display;

	window_set_user_data(view->window, view);
	window_set_redraw_handler(view->window, redraw_handler);
	window_set_resize_handler(view->window, resize_handler);
	window_set_key_handler(view->window, key_handler);
	window_set_keyboard_focus_handler(view->window,
					  keyboard_focus_handler);
	window_set_button_handler(view->window, button_handler);
	view->page = 0;

	view->fullscreen = fullscreen;
	window_set_fullscreen(view->window, view->fullscreen);

	view_draw(view);

	return view;
}

static int option_fullscreen;

static const GOptionEntry option_entries[] = {
	{ "fullscreen", 'f', 0, G_OPTION_ARG_NONE,
	  &option_fullscreen, "Run in fullscreen mode" },
	{ NULL }
};

int
main(int argc, char *argv[])
{
	struct display *d;
	int i;

	d = display_create(&argc, &argv, option_entries, NULL);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
		view_create (d, i, argv[i], option_fullscreen);

	display_run(d);

	return 0;
}

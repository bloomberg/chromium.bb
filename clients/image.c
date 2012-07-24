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
#include <libgen.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>

#include <wayland-client.h>

#include "window.h"
#include "../shared/cairo-util.h"

struct image {
	struct window *window;
	struct widget *widget;
	struct display *display;
	char *filename;
	cairo_surface_t *image;
	int fullscreen;
};

static void
redraw_handler(struct widget *widget, void *data)
{
	struct image *image = data;
	struct rectangle allocation;
	cairo_t *cr;
	cairo_surface_t *surface;
	double width, height, doc_aspect, window_aspect, scale;

	widget_get_allocation(image->widget, &allocation);

	surface = window_get_surface(image->window);
	cr = cairo_create(surface);
	widget_get_allocation(image->widget, &allocation);
	cairo_rectangle(cr, allocation.x, allocation.y,
			allocation.width, allocation.height);
	cairo_clip(cr);
	cairo_push_group(cr);
	cairo_translate(cr, allocation.x, allocation.y);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_paint(cr);

	width = cairo_image_surface_get_width(image->image);
	height = cairo_image_surface_get_height(image->image);
	doc_aspect = width / height;
	window_aspect = (double) allocation.width / allocation.height;
	if (doc_aspect < window_aspect)
		scale = allocation.height / height;
	else
		scale = allocation.width / width;
	cairo_scale(cr, scale, scale);
	cairo_translate(cr,
			(allocation.width - width * scale) / 2 / scale,
			(allocation.height - height * scale) / 2 / scale);

	cairo_set_source_surface(cr, image->image, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_paint(cr);

	cairo_pop_group_to_source(cr);
	cairo_paint(cr);
	cairo_destroy(cr);

	cairo_surface_destroy(surface);
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct image *image = data;

	window_schedule_redraw(image->window);
}

static void
fullscreen_handler(struct window *window, void *data)
{
	struct image *image = data;

	image->fullscreen ^= 1;
	window_set_fullscreen(window, image->fullscreen);
}

static struct image *
image_create(struct display *display, const char *filename)
{
	struct image *image;
	char *b, *copy, title[512];;

	image = malloc(sizeof *image);
	if (image == NULL)
		return image;
	memset(image, 0, sizeof *image);

	copy = strdup(filename);
	b = basename(copy);
	snprintf(title, sizeof title, "Wayland Image - %s", b);
	free(copy);

	image->filename = strdup(filename);
	image->image = load_cairo_surface(filename);
	image->window = window_create(display);
	image->widget = frame_create(image->window, image);
	window_set_title(image->window, title);
	image->display = display;

	window_set_user_data(image->window, image);
	widget_set_redraw_handler(image->widget, redraw_handler);
	window_set_keyboard_focus_handler(image->window,
					  keyboard_focus_handler);
	window_set_fullscreen_handler(image->window, fullscreen_handler);

	widget_schedule_resize(image->widget, 500, 400);

	return image;
}

int
main(int argc, char *argv[])
{
	struct display *d;
	int i;

	d = display_create(argc, argv);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
		image_create (d, argv[i]);

	display_run(d);

	return 0;
}

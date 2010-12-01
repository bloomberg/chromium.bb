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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "wayland-client.h"
#include "wayland-glib.h"

#include "window.h"

struct image {
	struct window *window;
	struct display *display;
	uint32_t key;
	gchar *filename;
};

static void
set_source_pixbuf(cairo_t         *cr,
		  const GdkPixbuf *pixbuf,
		  double           src_x,
		  double           src_y,
		  double           src_width,
		  double           src_height)
{
	gint width = gdk_pixbuf_get_width (pixbuf);
	gint height = gdk_pixbuf_get_height (pixbuf);
	guchar *gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
	int gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	int cairo_stride;
	guchar *cairo_pixels;
	cairo_format_t format;
	cairo_surface_t *surface;
	int j;

	if (n_channels == 3)
		format = CAIRO_FORMAT_RGB24;
	else
		format = CAIRO_FORMAT_ARGB32;

	surface = cairo_image_surface_create(format, width, height);
	if (cairo_surface_status(surface)) {
		cairo_set_source_surface(cr, surface, 0, 0);
		return;
	}

	cairo_stride = cairo_image_surface_get_stride(surface);
	cairo_pixels = cairo_image_surface_get_data(surface);

	for (j = height; j; j--) {
		guchar *p = gdk_pixels;
		guchar *q = cairo_pixels;

		if (n_channels == 3) {
			guchar *end = p + 3 * width;

			while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				q[0] = p[2];
				q[1] = p[1];
				q[2] = p[0];
#else
				q[1] = p[0];
				q[2] = p[1];
				q[3] = p[2];
#endif
				p += 3;
				q += 4;
			}
		} else {
			guchar *end = p + 4 * width;
			guint t1,t2,t3;

#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END

			while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				MULT(q[0], p[2], p[3], t1);
				MULT(q[1], p[1], p[3], t2);
				MULT(q[2], p[0], p[3], t3);
				q[3] = p[3];
#else
				q[0] = p[3];
				MULT(q[1], p[0], p[3], t1);
				MULT(q[2], p[1], p[3], t2);
				MULT(q[3], p[2], p[3], t3);
#endif

				p += 4;
				q += 4;
			}
#undef MULT
		}

		gdk_pixels += gdk_rowstride;
		cairo_pixels += cairo_stride;
	}
	cairo_surface_mark_dirty(surface);

	cairo_set_source_surface(cr, surface,
				 src_x + .5 * (src_width - width),
				 src_y + .5 * (src_height - height));
	cairo_surface_destroy(surface);
}

static void
image_draw(struct image *image)
{
	struct rectangle rectangle;
	GdkPixbuf *pb;
	cairo_t *cr;
	cairo_surface_t *wsurface, *surface;

	window_draw(image->window);

	window_get_child_rectangle(image->window, &rectangle);

	pb = gdk_pixbuf_new_from_file_at_size(image->filename,
					      rectangle.width,
					      rectangle.height,
					      NULL);
	if (pb == NULL)
		return;

	wsurface = window_get_surface(image->window);
	surface = cairo_surface_create_similar(wsurface,
					       CAIRO_CONTENT_COLOR_ALPHA,
					       rectangle.width,
					       rectangle.height);

	cairo_surface_destroy(wsurface);
	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_paint(cr);
	set_source_pixbuf(cr, pb,
			  0, 0,
			  rectangle.width, rectangle.height);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_paint(cr);
	cairo_destroy(cr);

	g_object_unref(pb);

	window_copy_surface(image->window, &rectangle, surface);
	window_flush(image->window);
	cairo_surface_destroy(surface);
}

static void
redraw_handler(struct window *window, void *data)
{
	struct image *image = data;

	image_draw(image);
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct image *image = data;

	window_schedule_redraw(image->window);
}

static struct image *
image_create(struct display *display, uint32_t key, const char *filename)
{
	struct image *image;
	gchar *basename;
	gchar *title;

	image = malloc(sizeof *image);
	if (image == NULL)
		return image;
	memset(image, 0, sizeof *image);

	basename = g_path_get_basename(filename);
	title = g_strdup_printf("Wayland Image - %s", basename);
	g_free(basename);

	image->filename = g_strdup(filename);

	image->window = window_create(display, title, 100 * key, 100 * key, 500, 400);
	image->display = display;

	/* FIXME: Window uses key 1 for moves, need some kind of
	 * allocation scheme here.  Or maybe just a real toolkit. */
	image->key = key + 100;

	window_set_user_data(image->window, image);
	window_set_redraw_handler(image->window, redraw_handler);
	window_set_keyboard_focus_handler(image->window,
					  keyboard_focus_handler);

	image_draw(image);

	return image;
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
		struct image *image;

		image = image_create (d, i, argv[i]);
	}

	display_run(d);

	return 0;
}

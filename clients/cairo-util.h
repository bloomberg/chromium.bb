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

#ifndef _CAIRO_UTIL_H
#define _CAIRO_UTIL_H

void
surface_flush_device(cairo_surface_t *surface);

void
blur_surface(cairo_surface_t *surface, int margin);

void
tile_mask(cairo_t *cr, cairo_surface_t *surface,
	  int x, int y, int width, int height, int margin, int top_margin);

void
tile_source(cairo_t *cr, cairo_surface_t *surface,
	    int x, int y, int width, int height, int margin, int top_margin);

void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius);

cairo_surface_t *
load_cairo_surface(const char *filename);

struct theme {
	cairo_surface_t *active_frame;
	cairo_surface_t *inactive_frame;
	cairo_surface_t *shadow;
	int frame_radius;
	int margin;
	int width;
	int titlebar_height;
};

void
display_render_theme(struct theme *t);
void
fini_theme(struct theme *t);

#endif

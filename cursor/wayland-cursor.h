/*
 * Copyright © 2012 Intel Corporation
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

#ifndef WAYLAND_CURSOR_H
#define WAYLAND_CURSOR_H

#include <stdint.h>

enum wl_cursor_type {
	WL_CURSOR_BOTTOM_LEFT,
	WL_CURSOR_BOTTOM_RIGHT,
	WL_CURSOR_BOTTOM,
	WL_CURSOR_DRAGGING,
	WL_CURSOR_LEFT_PTR,
	WL_CURSOR_LEFT,
	WL_CURSOR_RIGHT,
	WL_CURSOR_TOP_LEFT,
	WL_CURSOR_TOP_RIGHT,
	WL_CURSOR_TOP,
	WL_CURSOR_IBEAM,
	WL_CURSOR_HAND1,
};

struct wl_cursor_theme;

struct wl_cursor_image {
	uint32_t width;		/* actual width */
	uint32_t height;	/* actual height */
	uint32_t hotspot_x;	/* hot spot x (must be inside image) */
	uint32_t hotspot_y;	/* hot spot y (must be inside image) */
	uint32_t delay;		/* animation delay to next frame (ms) */
};

struct wl_cursor {
	unsigned int image_count;
	struct wl_cursor_image **images;
	char *name;
};

struct wl_shm;

struct wl_cursor_theme *
wl_cursor_theme_load(const char *name, int size, struct wl_shm *shm);

void
wl_cursor_theme_destroy(struct wl_cursor_theme *theme);

struct wl_cursor *
wl_cursor_theme_get_cursor(struct wl_cursor_theme *theme,
			   enum wl_cursor_type type);

struct wl_cursor *
wl_cursor_theme_get_cursor_by_name(struct wl_cursor_theme *theme,
				   const char *name);

struct wl_buffer *
wl_cursor_image_get_buffer(struct wl_cursor_image *image);

#endif

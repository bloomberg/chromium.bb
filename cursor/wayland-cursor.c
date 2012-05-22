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

#include "xcursor.h"
#include "wayland-cursor.h"
#include "wayland-client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

struct shm_pool {
	struct wl_shm_pool *pool;
	int fd;
	unsigned int size;
	unsigned int used;
	char *data;
};

static struct shm_pool *
shm_pool_create(struct wl_shm *shm, int size)
{
	struct shm_pool *pool;
	char filename[] = "/tmp/wayland-shm-XXXXXX";

	pool = malloc(sizeof *pool);
	if (!pool)
		return NULL;

	pool->fd = mkstemp(filename);
	if (pool->fd < 0)
		return NULL;

	if (ftruncate(pool->fd, size) < 0) {
		close(pool->fd);
		return NULL;
	}

	pool->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			  pool->fd, 0);
	unlink(filename);

	if (pool->data == MAP_FAILED) {
		close(pool->fd);
		return NULL;
	}

	pool->pool = wl_shm_create_pool(shm, pool->fd, size);
	pool->size = size;
	pool->used = 0;

	return pool;
}

static int
shm_pool_resize(struct shm_pool *pool, int size)
{
	if (ftruncate(pool->fd, size) < 0)
		return 0;

	wl_shm_pool_resize(pool->pool, size);

	munmap(pool->data, pool->size);

	pool->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			  pool->fd, 0);
	pool->size = size;

	return 1;
}

static int
shm_pool_allocate(struct shm_pool *pool, int size)
{
	int offset;

	if (pool->used + size > pool->size)
		if (!shm_pool_resize(pool, 2 * pool->size + size))
			return -1;

	offset = pool->used;
	pool->used += size;

	return offset;
}

static void
shm_pool_destroy(struct shm_pool *pool)
{
	munmap(pool->data, pool->size);
	wl_shm_pool_destroy(pool->pool);
	free(pool);
}


static const char *cursor_names[] = {
	"bottom_left_corner",
	"bottom_right_corner",
	"bottom_side",
	"grabbing",
	"left_ptr",
	"left_side",
	"right_side",
	"top_left_corner",
	"top_right_corner",
	"top_side",
	"xterm",
	"hand1",
};

struct wl_cursor_theme {
	unsigned int cursor_count;
	struct wl_cursor **cursors;
	struct wl_shm *shm;
	struct shm_pool *pool;
	char *name;
	int size;
};

struct cursor_image {
	struct wl_cursor_image image;
	struct wl_cursor_theme *theme;
	struct wl_buffer *buffer;
	int offset; /* data offset of this image in the shm pool */
};

/** Get an shm buffer for a cursor image
 *
 * \param image The cursor image
 * \return An shm buffer for the cursor image. The user should not destroy
 * the returned buffer.
 */
WL_EXPORT struct wl_buffer *
wl_cursor_image_get_buffer(struct wl_cursor_image *_img)
{
	struct cursor_image *image = (struct cursor_image *) _img;
	struct wl_cursor_theme *theme = image->theme;

	if (!image->buffer) {
		image->buffer =
			wl_shm_pool_create_buffer(theme->pool->pool,
						  image->offset,
						  _img->width, _img->height,
						  _img->width * 4,
						  WL_SHM_FORMAT_ARGB8888);
	};

	return image->buffer;
}

static void
wl_cursor_image_destroy(struct wl_cursor_image *_img)
{
	struct cursor_image *image = (struct cursor_image *) _img;

	if (image->buffer)
		wl_buffer_destroy(image->buffer);

	free(image);
}

static void
wl_cursor_destroy(struct wl_cursor *cursor)
{
	unsigned int i;

	for (i = 0; i < cursor->image_count; i++)
		wl_cursor_image_destroy(cursor->images[i]);

	free(cursor->name);
	free(cursor);
}

static struct wl_cursor *
load_cursor(struct wl_cursor_theme *theme, const char *name)
{
	XcursorImages *images;
	struct wl_cursor *cursor;
	struct cursor_image *image;
	int i, size;

	images = XcursorLibraryLoadImages(name, theme->name, theme->size);
	if (!images)
		return NULL;

	cursor = malloc(sizeof *cursor);
	if (!cursor) {
		XcursorImagesDestroy(images);
		return NULL;
	}

	cursor->image_count = images->nimage;
	cursor->images = malloc(images->nimage * sizeof cursor->images[0]);
	if (!cursor->images) {
		XcursorImagesDestroy(images);
		free(cursor);
		return NULL;
	}

	cursor->name = strdup(name);

	for (i = 0; i < images->nimage; i++) {
		image = malloc(sizeof *image);
		cursor->images[i] = (struct wl_cursor_image *) image;

		image->theme = theme;
		image->buffer = NULL;

		image->image.width = images->images[i]->width;
		image->image.height = images->images[i]->height;
		image->image.hotspot_x = images->images[i]->xhot;
		image->image.hotspot_y = images->images[i]->yhot;
		image->image.delay = images->images[i]->delay;

		/* copy pixels to shm pool */
		size = image->image.width * image->image.height * 4;
		image->offset = shm_pool_allocate(theme->pool, size);
		memcpy(theme->pool->data + image->offset,
		       images->images[i]->pixels, size);
	}

	XcursorImagesDestroy(images);

	return cursor;
}

/** Load a cursor theme to memory shared with the compositor
 *
 * \param name The name of the cursor theme to load. If %NULL, the default
 * theme will be loaded.
 * \param size Desired size of the cursor images.
 * \param shm The compositor's shm interface.
 *
 * \return An object representing the theme that should be destroyed with
 * wl_cursor_theme_destroy() or %NULL on error.
 */
WL_EXPORT struct wl_cursor_theme *
wl_cursor_theme_load(const char *name, int size, struct wl_shm *shm)
{
	struct wl_cursor_theme *theme;
	unsigned int i;

	theme = malloc(sizeof *theme);
	if (!theme)
		return NULL;

	if (!name)
		name = "default";

	theme->name = strdup(name);
	theme->size = size;
	theme->cursor_count = ARRAY_LENGTH(cursor_names);

	theme->cursors =
		malloc(theme->cursor_count * sizeof theme->cursors[0]);
	if (!theme->cursors) {
		free(theme);
		return NULL;
	}

	theme->pool =
		shm_pool_create(shm, theme->cursor_count * size * size * 4);

	for (i = 0; i < theme->cursor_count; i++)
		theme->cursors[i] = load_cursor(theme, cursor_names[i]);

	return theme;
}

/** Destroys a cursor theme object
 *
 * \param theme The cursor theme to be destroyed
 */
WL_EXPORT void
wl_cursor_theme_destroy(struct wl_cursor_theme *theme)
{
	unsigned int i;

	for (i = 0; i < theme->cursor_count; i++)
		wl_cursor_destroy(theme->cursors[i]);

	shm_pool_destroy(theme->pool);

	free(theme->cursors);
	free(theme);
}

/** Get the cursor for a given type from a cursor theme
 *
 * \param theme The cursor theme
 * \patam type The desired cursor type
 * \return The theme's cursor of the given type or %NULL if there is no
 * such cursor
 */
WL_EXPORT struct wl_cursor *
wl_cursor_theme_get_cursor(struct wl_cursor_theme *theme,
			   enum wl_cursor_type type)
{
	if (type < theme->cursor_count)
		return theme->cursors[type];

	return NULL;
}

/** Get the cursor for a given name from a cursor theme
 *
 * \param theme The cursor theme
 * \param name Name of the desired cursor
 * \return The theme's cursor of the given name or %NULL if there is no
 * such cursor
 */
WL_EXPORT struct wl_cursor *
wl_cursor_theme_get_cursor_by_name(struct wl_cursor_theme *theme,
				   const char *name)
{
	unsigned int i;
	struct wl_cursor *cursor;

	for (i = 0; i < theme->cursor_count; i++) {
		if (strcmp(name, theme->cursors[i]->name) == 0)
			return theme->cursors[i];
	}

	cursor = load_cursor(theme, name);
	if (!cursor)
		return NULL;

	theme->cursor_count++;
	theme->cursors =
		realloc(theme->cursors,
			theme->cursor_count * sizeof theme->cursors[0]);

	theme->cursors[theme->cursor_count - 1] = cursor;

	return cursor;
}

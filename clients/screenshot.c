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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cairo.h>

#include <wayland-client.h>
#include "screenshooter-client-protocol.h"

#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

/* The screenshooter is a good example of a custom object exposed by
 * the compositor and serves as a test bed for implementing client
 * side marshalling outside libwayland.so */

static struct wl_shm *shm;
static struct screenshooter *screenshooter;
static struct wl_list output_list;
int buffer_copy_done;

struct screenshooter_output {
	struct wl_output *output;
	struct wl_buffer *buffer;
	int width, height, offset_x, offset_y;
	void *data;
	struct wl_list link;
};

static void
display_handle_geometry(void *data,
			struct wl_output *wl_output,
			int x,
			int y,
			int physical_width,
			int physical_height,
			int subpixel,
			const char *make,
			const char *model)
{
	struct screenshooter_output *output;

	output = wl_output_get_user_data(wl_output);

	if (wl_output == output->output) {
		output->offset_x = x;
		output->offset_y = y;
	}
}

static void
display_handle_mode(void *data,
		    struct wl_output *wl_output,
		    uint32_t flags,
		    int width,
		    int height,
		    int refresh)
{
	struct screenshooter_output *output;

	output = wl_output_get_user_data(wl_output);

	if (wl_output == output->output && (flags & WL_OUTPUT_MODE_CURRENT)) {
		output->width = width;
		output->height = height;
	}
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
	display_handle_mode
};

static void
screenshot_done(void *data, struct screenshooter *screenshooter)
{
	buffer_copy_done = 1;
}

static const struct screenshooter_listener screenshooter_listener = {
	screenshot_done
};

static void
handle_global(struct wl_display *display, uint32_t id,
	      const char *interface, uint32_t version, void *data)
{
	static struct screenshooter_output *output;

	if (strcmp(interface, "wl_output") == 0) {
		output = malloc(sizeof *output);
		output->output = wl_display_bind(display, id, &wl_output_interface);
		wl_list_insert(&output_list, &output->link);
		wl_output_add_listener(output->output, &output_listener, output);
	} else if (strcmp(interface, "wl_shm") == 0) {
		shm = wl_display_bind(display, id, &wl_shm_interface);
	} else if (strcmp(interface, "screenshooter") == 0) {
		screenshooter = wl_display_bind(display, id, &screenshooter_interface);
	}
}

static struct wl_buffer *
create_shm_buffer(int width, int height, void **data_out)
{
	char filename[] = "/tmp/wayland-shm-XXXXXX";
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
	int fd, size, stride;
	void *data;

	fd = mkstemp(filename);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %m\n", filename);
		return NULL;
	}
	stride = width * 4;
	size = stride * height;
	if (ftruncate(fd, size) < 0) {
		fprintf(stderr, "ftruncate failed: %m\n");
		close(fd);
		return NULL;
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	unlink(filename);

	if (data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		return NULL;
	}

	pool = wl_shm_create_pool(shm, fd, size);
	close(fd);
	buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
					   WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);

	*data_out = data;

	return buffer;
}

static void
write_png(int width, int height)
{
	int output_stride, buffer_stride, i;
	cairo_surface_t *surface;
	void *data, *d, *s;
	struct screenshooter_output *output, *next;

	buffer_stride = width * 4;

	data = malloc(buffer_stride * height);
	if (!data)
		return;

	wl_list_for_each_safe(output, next, &output_list, link) {
		output_stride = output->width * 4;
		s = output->data;
		d = data + output->offset_y * buffer_stride + output->offset_x * 4;

		for (i = 0; i < output->height; i++) {
			memcpy(d, s, output_stride);
			d += buffer_stride;
			s += output_stride;
		}

		free(output);
	}

	surface = cairo_image_surface_create_for_data(data,
						      CAIRO_FORMAT_ARGB32,
						      width, height, buffer_stride);
	cairo_surface_write_to_png(surface, "wayland-screenshot.png");
	cairo_surface_destroy(surface);
	free(data);
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct screenshooter_output *output;
	int width = 0, height = 0;

	display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	wl_list_init(&output_list);
	wl_display_add_global_listener(display, handle_global, &screenshooter);
	wl_display_iterate(display, WL_DISPLAY_READABLE);
	wl_display_roundtrip(display);
	if (screenshooter == NULL) {
		fprintf(stderr, "display doesn't support screenshooter\n");
		return -1;
	}

	screenshooter_add_listener(screenshooter, &screenshooter_listener, screenshooter);


	wl_list_for_each(output, &output_list, link) {
		output->buffer = create_shm_buffer(output->width, output->height, &output->data);
		screenshooter_shoot(screenshooter, output->output, output->buffer);
		width = MAX(width, output->offset_x + output->width);
		height = MAX(height, output->offset_y + output->height);
		buffer_copy_done = 0;
		while (!buffer_copy_done)
			wl_display_roundtrip(display);
	}

	write_png(width, height);

	return 0;
}

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
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "wayland-client.h"
#include "wayland-glib.h"
#include "screenshooter-client-protocol.h"

/* The screenshooter is a good example of a custom object exposed by
 * the compositor and serves as a test bed for implementing client
 * side marshalling outside libwayland.so */

static struct wl_output *output;
static struct wl_shm *shm;
static struct screenshooter *screenshooter;
static int output_width, output_height;

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
}

static void
display_handle_mode(void *data,
		    struct wl_output *wl_output,
		    uint32_t flags,
		    int width,
		    int height,
		    int refresh)
{
	output_width = width;
	output_height = height;
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
	display_handle_mode
};

static void
handle_global(struct wl_display *display, uint32_t id,
	      const char *interface, uint32_t version, void *data)
{
	if (strcmp(interface, "wl_output") == 0) {
		output = wl_display_bind(display, id, &wl_output_interface);
		wl_output_add_listener(output, &output_listener, NULL);
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

	buffer = wl_shm_create_buffer(shm, fd, width, height, stride,
				      WL_SHM_FORMAT_XRGB32);

	close(fd);

	*data_out = data;

	return buffer;
}

static void
write_png(int width, int height, void *data)
{
	GdkPixbuf *pixbuf, *normal;
	GError *error = NULL;

	g_type_init();
	pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE,
		                          8, width, height, width * 4, NULL,
	                                  NULL);

	normal = gdk_pixbuf_flip(pixbuf, FALSE);
	gdk_pixbuf_save(normal, "wayland-screenshot.png", "png", &error, NULL);
	g_object_unref(normal);
	g_object_unref(pixbuf);
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wl_buffer *buffer;
	void *data = NULL;

	display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	wl_display_add_global_listener(display, handle_global, &screenshooter);
	wl_display_iterate(display, WL_DISPLAY_READABLE);
	wl_display_roundtrip(display);
	if (screenshooter == NULL) {
		fprintf(stderr, "display doesn't support screenshooter\n");
		return -1;
	}

	buffer = create_shm_buffer(output_width, output_height, &data);
	screenshooter_shoot(screenshooter, output, buffer);
	wl_display_roundtrip(display);

	write_png(output_width, output_height, data);

	return 0;
}

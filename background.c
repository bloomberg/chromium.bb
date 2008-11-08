#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

#include "wayland-client.h"
#include "wayland-glib.h"

static const char gem_device[] = "/dev/dri/card0";
static const char socket_name[] = "\0wayland";

static uint8_t *convert_to_argb(GdkPixbuf *pixbuf, int *new_stride)
{
	uint8_t *data, *row, *src;
	uint32_t *pixel;
	int width, height, stride, channels, i, j;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	stride = gdk_pixbuf_get_rowstride(pixbuf);
	row = gdk_pixbuf_get_pixels(pixbuf);
	channels = gdk_pixbuf_get_n_channels(pixbuf);

	data = malloc(height * width * 4);
	if (data == NULL) {
		fprintf(stderr, "out of memory\n");
		return NULL;
	}

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			src = &row[i * stride + j * channels];
			pixel = (uint32_t *) &data[i * width * 4 + j * 4];
			*pixel = 0xff000000 | (src[0] << 16) | (src[1] << 8) | src[2];
		}
	}

	*new_stride = width * 4;

	return data;
}

static uint32_t name_pixbuf(int fd, GdkPixbuf *pixbuf)
{
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	struct drm_i915_gem_pwrite pwrite;
	int32_t width, height, stride;
	void *data;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	data = convert_to_argb(pixbuf, &stride);

	memset(&create, 0, sizeof(create));
	create.size = height * stride;

	if (ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
		fprintf(stderr, "gem create failed: %m\n");
		return 0;
	}

	pwrite.handle = create.handle;
	pwrite.offset = 0;
	pwrite.size = height * stride;
	pwrite.data_ptr = (uint64_t) (uintptr_t) data;
	if (ioctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite) < 0) {
		fprintf(stderr, "gem pwrite failed: %m\n");
		return 0;
	}

	flink.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
		fprintf(stderr, "gem flink failed: %m\n");
		return 0;
	}

#if 0
	/* We need to hold on to the handle until the server has received
	 * the attach request... we probably need a confirmation event.
	 * I guess the breadcrumb idea will suffice. */
	struct drm_gem_close close;
	close.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close) < 0) {
		fprintf(stderr, "gem close failed: %m\n");
		return 0;
	}
#endif

	return flink.name;
}

int main(int argc, char *argv[])
{
	GdkPixbuf *image;
	GError *error = NULL;
	struct wl_display *display;
	struct wl_surface *surface;
	int fd, width, height, stride;
	uint32_t name;
	GMainLoop *loop;
	GSource *source;

	fd = open(gem_device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return -1;
	}

	display = wl_display_create(socket_name);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	loop = g_main_loop_new(NULL, FALSE);
	source = wayland_source_new(display);
	g_source_attach(source, NULL);

	surface = wl_display_create_surface(display);

	g_type_init();
	image = gdk_pixbuf_new_from_file (argv[1], &error);

	name = name_pixbuf(fd, image);

	width = gdk_pixbuf_get_width(image);
	height = gdk_pixbuf_get_height(image);
	stride = gdk_pixbuf_get_rowstride(image);

	wl_surface_attach(surface, name, width, height, width * 4);

	wl_surface_map(surface, 0, 0, 1280, 800);

	g_main_loop_run(loop);

	return 0;
}

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <math.h>
#include <cairo.h>
#include "cairo-util.h"

struct buffer *
buffer_create(int fd, int width, int height, int stride)
{
	struct buffer *buffer;
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;

	buffer = malloc(sizeof *buffer);
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;

	memset(&create, 0, sizeof(create));
	create.size = height * stride;

	if (ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
		fprintf(stderr, "gem create failed: %m\n");
		free(buffer);
		return NULL;
	}

	flink.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
		fprintf(stderr, "gem flink failed: %m\n");
		free(buffer);
		return 0;
	}

	buffer->handle = flink.handle;
	buffer->name = flink.name;

	return buffer;
}

int
buffer_destroy(struct buffer *buffer, int fd)
{
	struct drm_gem_close close;

	close.handle = buffer->handle;
	if (ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close) < 0) {
		fprintf(stderr, "gem close failed: %m\n");
		return -1;
	}
	
	free(buffer);

	return 0;
}

int
buffer_data(struct buffer *buffer, int fd, void *data)
{
	struct drm_i915_gem_pwrite pwrite;

	pwrite.handle = buffer->handle;
	pwrite.offset = 0;
	pwrite.size = buffer->height * buffer->stride;
	pwrite.data_ptr = (uint64_t) (uintptr_t) data;

	if (ioctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite) < 0) {
		fprintf(stderr, "gem pwrite failed: %m\n");
		return -1;
	}

	return 0;
}

struct buffer *
buffer_create_from_cairo_surface(int fd, cairo_surface_t *surface)
{
	struct buffer *buffer;
	int32_t width, height, stride;
	void *data;

	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	buffer = buffer_create(fd, width, height, stride);
	if (buffer == NULL)
		return NULL;

	if (buffer_data(buffer, fd, data) < 0) {
		buffer_destroy(buffer, fd);
		return NULL;
	}			

	return buffer;
}

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

void
blur_surface(cairo_surface_t *surface, int margin)
{
	cairo_surface_t *tmp;
	int32_t width, height, stride, x, y, z, w;
	uint8_t *src, *dst;
	uint32_t *s, *d, a, p;
	int i, j, k, size, half;
	uint8_t kernel[17];
	double f;

	size = ARRAY_LENGTH(kernel);
	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);
	src = cairo_image_surface_get_data(surface);

	tmp = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
	dst = cairo_image_surface_get_data(tmp);

	half = size / 2;
	a = 0;
	for (i = 0; i < size; i++) {
		f = (i - half);
		kernel[i] = exp(- f * f / 30.0) * 80;
		a += kernel[i];
	}

	for (i = 0; i < height; i++) {
		s = (uint32_t *) (src + i * stride);
		d = (uint32_t *) (dst + i * stride);
		for (j = 0; j < width; j++) {
			if (margin < j && j < width - margin) {
				d[j] = s[j];
				continue;
			}

			x = 0;
			y = 0;
			z = 0;
			w = 0;
			for (k = 0; k < size; k++) {
				if (j - half + k < 0 || j - half + k >= width)
					continue;
				p = s[j - half + k];

				x += (p >> 24) * kernel[k];
				y += ((p >> 16) & 0xff) * kernel[k];
				z += ((p >> 8) & 0xff) * kernel[k];
				w += (p & 0xff) * kernel[k];
			}
			d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
		}
	}

	for (i = 0; i < height; i++) {
		s = (uint32_t *) (dst + i * stride);
		d = (uint32_t *) (src + i * stride);
		for (j = 0; j < width; j++) {
			if (margin <= i && i < height - margin) {
				d[j] = s[j];
				continue;
			}

			x = 0;
			y = 0;
			z = 0;
			w = 0;
			for (k = 0; k < size; k++) {
				if (i - half + k < 0 || i - half + k >= height)
					continue;
				s = (uint32_t *) (dst + (i - half + k) * stride);
				p = s[j];

				x += (p >> 24) * kernel[k];
				y += ((p >> 16) & 0xff) * kernel[k];
				z += ((p >> 8) & 0xff) * kernel[k];
				w += (p & 0xff) * kernel[k];
			}
			d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
		}
	}

	cairo_surface_destroy(tmp);
}

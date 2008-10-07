#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>

#include "wayland.h"

struct lame_compositor {
	struct wl_compositor base;
	void *fb;
	int32_t width, height, stride;
	int gem_fd;
};

void notify_surface_create(struct wl_compositor *compositor,
			   struct wl_surface *surface)
{
}
				   
void notify_surface_attach(struct wl_compositor *compositor,
			   struct wl_surface *surface, uint32_t name, 
			   uint32_t width, uint32_t height, uint32_t stride)
{
	struct lame_compositor *lc = (struct lame_compositor *) compositor;
	struct drm_i915_gem_pread pread;
	struct drm_gem_open open_arg;
	struct drm_gem_close close_arg;
	char *data, *dst;
	int i, ret, x = 600, y = 200;

	open_arg.name = name;
	ret = ioctl(lc->gem_fd, DRM_IOCTL_GEM_OPEN, &open_arg);
	if (ret != 0) {
		fprintf(stderr, "failed to gem_open name %d, fd=%d: %m\n", name, lc->gem_fd);
		return;
	}

	data = malloc(open_arg.size);
	if (data == NULL) {
		fprintf(stderr, "swap buffers malloc failed\n");
		return;
	}

	pread.size = open_arg.size;
	pread.handle = open_arg.handle;
	pread.pad = 0;
	pread.offset = 0;
	pread.size = stride * height;
	pread.data_ptr = (long) data;

	if (ioctl(lc->gem_fd, DRM_IOCTL_I915_GEM_PREAD, &pread)) {
		fprintf(stderr, "gem pread failed");
		return;
	}

	dst = lc->fb + lc->stride * y + x * 4;
	for (i = 0; i < height; i++)
		memcpy(dst + lc->stride * i, data + stride * i, width * 4);

	close_arg.handle = open_arg.handle;
	ret = ioctl(lc->gem_fd, DRM_IOCTL_GEM_CLOSE, &close_arg);
	if (ret != 0) {
		fprintf(stderr, "failed to gem_close name %d: %m\n", name);
	}

	free(data);
}

struct wl_compositor_interface interface = {
	notify_surface_create,
	notify_surface_attach
};

static const char fb_device[] = "/dev/fb";
static const char gem_device[] = "/dev/dri/card0";

struct wl_compositor *
wl_compositor_create(void)
{
	struct lame_compositor *lc;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	int fd;

	lc = malloc(sizeof *lc);
	if (lc == NULL)
		return NULL;

	lc->base.interface = &interface;

	fd = open(fb_device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %m\n", fb_device);
		return NULL;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		fprintf(stderr, "fb get fixed failed\n");
		return NULL;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0) {
		fprintf(stderr, "fb get fixed failed\n");
		return NULL;
	}

	lc->stride = fix.line_length;
	lc->width = var.xres;
	lc->height = var.yres;
	lc->fb = mmap(NULL, lc->stride * lc->height,
		      PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (lc->fb == MAP_FAILED) {
		fprintf(stderr, "fb map failed\n");
		return NULL;
	}

	lc->gem_fd = open(gem_device, O_RDWR);
	if (lc->gem_fd < 0) {
		fprintf(stderr, "failed to open drm device\n");
		return NULL;
	}

	return &lc->base;
}

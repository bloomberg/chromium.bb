#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <png.h>

#include "wayland.h"

#include <GL/gl.h>
#include <eagle.h>

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct egl_compositor {
	struct wl_compositor base;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGLConfig config;
	struct wl_display *wl_display;
	int gem_fd;
	int width, height;
};

struct surface_data {
	GLuint texture;
	struct wl_map map;
	EGLSurface surface;
};

static int do_screenshot;

static void
handle_sigusr1(int s)
{
	do_screenshot = 1;
}

static void
die(const char *msg, ...)
{
	va_list ap;

	va_start (ap, msg);
	vfprintf(stderr, msg, ap);
	va_end (ap);

	exit(EXIT_FAILURE);
}

static void
stdio_write_func (png_structp png, png_bytep data, png_size_t size)
{
	FILE *fp;
	size_t ret;

	fp = png_get_io_ptr (png);
	while (size) {
		ret = fwrite (data, 1, size, fp);
		size -= ret;
		data += ret;
		if (size && ferror (fp))
			die("write: %m\n");
	}
}

static void
png_simple_output_flush_fn (png_structp png_ptr)
{
}

static void
png_simple_error_callback (png_structp png,
	                   png_const_charp error_msg)
{
	die("png error: %s\n", error_msg);
}

static void
png_simple_warning_callback (png_structp png,
	                     png_const_charp error_msg)
{
	fprintf(stderr, "png warning: %s\n", error_msg);
}

static void
convert_pixels(png_structp png, png_row_infop row_info, png_bytep data)
{
	unsigned int i;

	for (i = 0; i < row_info->rowbytes; i += 4) {
		uint8_t *b = &data[i];
		uint32_t pixel;

		memcpy (&pixel, b, sizeof (uint32_t));
		b[0] = (pixel & 0xff0000) >> 16;
		b[1] = (pixel & 0x00ff00) >>  8;
		b[2] = (pixel & 0x0000ff) >>  0;
		b[3] = 0;
	}
}

static void
screenshot(struct egl_compositor *ec)
{
	png_struct *png;
	png_info *info;
	png_byte **volatile rows = NULL;
	png_color_16 white;
	int depth, i;
	FILE *fp;
	uint8_t *data;
	GLuint stride;
	static const char filename[]  = "wayland-screenshot.png";

	data = eglReadBuffer(ec->display, ec->surface, GL_FRONT_LEFT, &stride);
	if (data == NULL)
		die("eglReadBuffer failed\n");
	rows = malloc(ec->height * sizeof rows[0]);
	if (rows == NULL)
		die("malloc failed\n");

	for (i = 0; i < ec->height; i++)
		rows[i] = (png_byte *) data + i * stride;

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
				      png_simple_error_callback,
				      png_simple_warning_callback);
	if (png == NULL)
		die("png_create_write_struct failed\n");

	info = png_create_info_struct(png);
	if (info == NULL)
		die("png_create_info_struct failed\n");

	fp = fopen(filename, "w");
	if (fp == NULL)
		die("fopen failed: %m\n");

	png_set_write_fn(png, fp, stdio_write_func, png_simple_output_flush_fn);

	depth = 8;
	png_set_IHDR(png, info,
		     ec->width,
		     ec->height, depth,
		     PNG_COLOR_TYPE_RGB,
		     PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT,
		     PNG_FILTER_TYPE_DEFAULT);

	white.gray = (1 << depth) - 1;
	white.red = white.blue = white.green = white.gray;
	png_set_bKGD(png, info, &white);
	png_write_info (png, info);
	png_set_write_user_transform_fn(png, convert_pixels);

	png_set_filler(png, 0, PNG_FILLER_AFTER);
	png_write_image(png, rows);
	png_write_end(png, info);

	png_destroy_write_struct(&png, &info);
	fclose(fp);
	free(rows);
	free(data);
}

static void
repaint(void *data)
{
	struct egl_compositor *ec = data;
	struct wl_surface_iterator *iterator;
	struct wl_surface *surface;
	struct surface_data *sd;
	GLint vertices[12];
	GLint tex_coords[12] = { 0, 0,  0, 1,  1, 0,  1, 1 };
	GLuint indices[4] = { 0, 1, 2, 3 };

	iterator = wl_surface_iterator_create(ec->wl_display, 0);
	while (wl_surface_iterator_next(iterator, &surface)) {
		sd = wl_surface_get_data(surface);
		if (sd == NULL)
			continue;

		vertices[0] = sd->map.x;
		vertices[1] = sd->map.y;
		vertices[2] = 0;

		vertices[3] = sd->map.x;
		vertices[4] = sd->map.y + sd->map.height;
		vertices[5] = 0;

		vertices[6] = sd->map.x + sd->map.width;
		vertices[7] = sd->map.y;
		vertices[8] = 0;

		vertices[9] = sd->map.x + sd->map.width;
		vertices[10] = sd->map.y + sd->map.height;
		vertices[11] = 0;

		glBindTexture(GL_TEXTURE_2D, sd->texture);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		/* Assume pre-multiplied alpha for now, this probably
		 * needs to be a wayland visual type of thing. */
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glVertexPointer(3, GL_INT, 0, vertices);
		glTexCoordPointer(2, GL_INT, 0, tex_coords);
		glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, indices);
	}
	wl_surface_iterator_destroy(iterator);

	eglSwapBuffers(ec->display, ec->surface);

	if (do_screenshot) {
		glFinish();
		/* FIXME: There's a bug somewhere so that glFinish()
		 * doesn't actually wait for all rendering to finish.
		 * I *think* it's fixed in upstream drm, but for my
		 * kernel I need this sleep now... */
		sleep(1);
		screenshot(ec);
		do_screenshot = 0;
	}
}

static void
schedule_repaint(struct egl_compositor *ec)
{
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(ec->wl_display);
	wl_event_loop_add_idle(loop, repaint, ec);
}

static void
notify_surface_create(struct wl_compositor *compositor,
		      struct wl_surface *surface)
{
	struct surface_data *sd;

	sd = malloc(sizeof *sd);
	if (sd == NULL)
		return;

	sd->surface = EGL_NO_SURFACE;
	wl_surface_set_data(surface, sd);

	glGenTextures(1, &sd->texture);
}
				   
static void
notify_surface_destroy(struct wl_compositor *compositor,
		       struct wl_surface *surface)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct surface_data *sd;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	if (sd->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, sd->surface);

	glDeleteTextures(1, &sd->texture);

	free(sd);

	schedule_repaint(ec);
}

static void
notify_surface_attach(struct wl_compositor *compositor,
		      struct wl_surface *surface, uint32_t name, 
		      uint32_t width, uint32_t height, uint32_t stride)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct surface_data *sd;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	if (sd->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, sd->surface);

	/* FIXME: We need to use a single buffer config without depth
	 * or stencil buffers here to keep egl from creating auxillary
	 * buffers for the pixmap here. */
	sd->surface = eglCreateSurfaceForName(ec->display, ec->config,
					      name, width, height, stride, NULL);

	glBindTexture(GL_TEXTURE_2D, sd->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	eglBindTexImage(ec->display, sd->surface, GL_TEXTURE_2D);

	schedule_repaint(ec);
}

static void
notify_surface_map(struct wl_compositor *compositor,
		   struct wl_surface *surface, struct wl_map *map)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct surface_data *sd;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	sd->map = *map;

	schedule_repaint(ec);
}

static void
notify_surface_copy(struct wl_compositor *compositor,
		    struct wl_surface *surface,
		    int32_t dst_x, int32_t dst_y,
		    uint32_t name, uint32_t stride,
		    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	EGLSurface src;
	struct surface_data *sd;

	sd = wl_surface_get_data(surface);

	/* FIXME: glCopyPixels should work, but then we'll have to
	 * call eglMakeCurrent to set up the src and dest surfaces
	 * first.  This seems cheaper, but maybe there's a better way
	 * to accomplish this. */

	src = eglCreateSurfaceForName(ec->display, ec->config,
				      name, x + width, y + height, stride, NULL);

	eglCopyNativeBuffers(ec->display, sd->surface, GL_FRONT_LEFT, dst_x, dst_y,
			     src, GL_FRONT_LEFT, x, y, width, height);
	schedule_repaint(ec);
}

static void
notify_surface_damage(struct wl_compositor *compositor,
		      struct wl_surface *surface,
		      int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;

	/* FIXME: This need to take a damage region, of course. */
	schedule_repaint(ec);
}

static const struct wl_compositor_interface interface = {
	notify_surface_create,
	notify_surface_destroy,
	notify_surface_attach,
	notify_surface_map,
	notify_surface_copy,
	notify_surface_damage
};

static const char gem_device[] = "/dev/dri/card0";

WL_EXPORT struct wl_compositor *
wl_compositor_create(struct wl_display *display)
{
	EGLConfig configs[64];
	EGLint major, minor, count;
	struct egl_compositor *ec;

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	ec->width = 1280;
	ec->height = 800;

	ec->base.interface = &interface;
	ec->wl_display = display;

	ec->display = eglCreateDisplayNative(gem_device, "i965");
	if (ec->display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return NULL;
	}

	if (!eglInitialize(ec->display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return NULL;
	}

	if (!eglGetConfigs(ec->display, configs, ARRAY_LENGTH(configs), &count)) {
		fprintf(stderr, "failed to get configs\n");
		return NULL;
	}

	ec->config = configs[24];
	ec->surface = eglCreateSurfaceNative(ec->display, ec->config,
					     0, 0, ec->width, ec->height);
	if (ec->surface == NULL) {
		fprintf(stderr, "failed to create surface\n");
		return NULL;
	}

	ec->context = eglCreateContext(ec->display, ec->config, NULL, NULL);
	if (ec->context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return NULL;
	}

	if (!eglMakeCurrent(ec->display, ec->surface, ec->surface, ec->context)) {
		fprintf(stderr, "failed to make context current\n");
		return NULL;
	}

	glViewport(0, 0, ec->width, ec->height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, ec->width, ec->height, 0, 0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glClearColor(0.0, 0.05, 0.2, 0.0);

	ec->gem_fd = open(gem_device, O_RDWR);
	if (ec->gem_fd < 0) {
		fprintf(stderr, "failed to open drm device\n");
		return NULL;
	}

	signal(SIGUSR1, handle_sigusr1);

	schedule_repaint(ec);

	return &ec->base;
}

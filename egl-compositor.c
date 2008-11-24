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
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <png.h>
#include <math.h>
#include <linux/input.h>

#include "wayland.h"
#include "cairo-util.h"

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
	struct egl_surface *pointer;
	struct egl_surface *background;
	struct egl_surface *overlay;
};

struct egl_surface {
	GLuint texture;
	struct wl_map map;
	EGLSurface surface;
};

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

struct screenshooter {
	struct wl_object base;
	struct egl_compositor *ec;
};

static void
screenshooter_shoot(struct wl_client *client, struct screenshooter *shooter)
{
	struct egl_compositor *ec = shooter->ec;
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

static const struct wl_method screenshooter_methods[] = {
	{ "shoot", screenshooter_shoot, 0, NULL }
};

static const struct wl_interface screenshooter_interface = {
	"screenshooter", 1,
	ARRAY_LENGTH(screenshooter_methods),
	screenshooter_methods,
};

static struct screenshooter *
screenshooter_create(struct egl_compositor *ec)
{
	struct screenshooter *shooter;

	shooter = malloc(sizeof *shooter);
	if (shooter == NULL)
		return NULL;

	shooter->base.interface = &screenshooter_interface;
	shooter->ec = ec;

	return shooter;
};

static struct egl_surface *
egl_surface_create_from_cairo_surface(cairo_surface_t *surface,
				      int x, int y, int width, int height)
{
	struct egl_surface *es;
	int stride;
	void *data;

	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	es = malloc(sizeof *es);
	if (es == NULL)
		return NULL;

	glGenTextures(1, &es->texture);
	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		     GL_BGRA, GL_UNSIGNED_BYTE, data);

	es->map.x = x;
	es->map.y = y;
	es->map.width = width;
	es->map.height = height;
	es->surface = EGL_NO_SURFACE;

	return es;
}

static void
egl_surface_destroy(struct egl_surface *es, struct egl_compositor *ec)
{
	glDeleteTextures(1, &es->texture);
	if (es->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, es->surface);
	free(es);
}

static void
pointer_path(cairo_t *cr, int x, int y)
{
	const int end = 3, tx = 4, ty = 12, dx = 5, dy = 10;
	const int width = 16, height = 16;

	cairo_move_to(cr, x, y);
	cairo_line_to(cr, x + tx, y + ty);
	cairo_line_to(cr, x + dx, y + dy);
	cairo_line_to(cr, x + width - end, y + height);
	cairo_line_to(cr, x + width, y + height - end);
	cairo_line_to(cr, x + dy, y + dx);
	cairo_line_to(cr, x + ty, y + tx);
	cairo_close_path(cr);
}

static struct egl_surface *
pointer_create(int x, int y, int width, int height)
{
	struct egl_surface *es;
	const int hotspot_x = 16, hotspot_y = 16;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					     width, height);

	cr = cairo_create(surface);
	pointer_path(cr, hotspot_x + 5, hotspot_y + 4);
	cairo_set_line_width (cr, 2);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);
	blur_surface(surface, width);

	pointer_path(cr, hotspot_x, hotspot_y);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	cairo_destroy(cr);

	es = egl_surface_create_from_cairo_surface(surface, x, y, width, height);
	
	cairo_surface_destroy(surface);

	return es;
}

static struct egl_surface *
background_create(const char *filename, int width, int height)
{
	struct egl_surface *background;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	int pixbuf_width, pixbuf_height;
	void *data;

	background = malloc(sizeof *background);
	if (background == NULL)
		return NULL;
	
	g_type_init();

	pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if (error != NULL) {
		free(background);
		return NULL;
	}

	pixbuf_width = gdk_pixbuf_get_width(pixbuf);
	pixbuf_height = gdk_pixbuf_get_height(pixbuf);
	data = gdk_pixbuf_get_pixels(pixbuf);

	glGenTextures(1, &background->texture);
	glBindTexture(GL_TEXTURE_2D, background->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pixbuf_width, pixbuf_height, 0,
		     GL_BGR, GL_UNSIGNED_BYTE, data);

	background->map.x = 0;
	background->map.y = 0;
	background->map.width = width;
	background->map.height = height;
	background->surface = EGL_NO_SURFACE;

	return background;
}

static void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius)
{
	cairo_move_to(cr, x0, y0 + radius);
	cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, 3 * M_PI / 2);
	cairo_line_to(cr, x1 - radius, y0);
	cairo_arc(cr, x1 - radius, y0 + radius, radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to(cr, x1, y1 - radius);
	cairo_arc(cr, x1 - radius, y1 - radius, radius, 0, M_PI / 2);
	cairo_line_to(cr, x0 + radius, y1);
	cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI / 2, M_PI);
	cairo_close_path(cr);
}

static void
draw_button(cairo_t *cr, int x, int y, int width, int height, const char *text)
{
	cairo_pattern_t *gradient;
	cairo_text_extents_t extents;
	double bright = 0.15, dim = 0.02;
	int radius = 10;

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_line_width (cr, 2);
	rounded_rect(cr, x, y, x + width, y + height, radius);
	cairo_set_source_rgb(cr, dim, dim, dim);
	cairo_stroke(cr);
	rounded_rect(cr, x + 2, y + 2, x + width, y + height, radius);
	cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
	cairo_stroke(cr);

	rounded_rect(cr, x + 1, y + 1, x + width - 1, y + height - 1, radius - 1);
	cairo_set_source_rgb(cr, bright, bright, bright);
	cairo_stroke(cr);
	rounded_rect(cr, x + 3, y + 3, x + width - 1, y + height - 1, radius - 1);
	cairo_set_source_rgb(cr, dim, dim, dim);
	cairo_stroke(cr);

	rounded_rect(cr, x + 1, y + 1, x + width - 1, y + height - 1, radius - 1);
	gradient = cairo_pattern_create_linear (0, y, 0, y + height);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 0.15, 0.15, 0.15);
	cairo_pattern_add_color_stop_rgb(gradient, 0.5, 0.08, 0.08, 0.08);
	cairo_pattern_add_color_stop_rgb(gradient, 0.5, 0.07, 0.07, 0.07);
	cairo_pattern_add_color_stop_rgb(gradient, 1, 0.1, 0.1, 0.1);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);

	cairo_set_font_size(cr, 16);
	cairo_text_extents(cr, text, &extents);
	cairo_move_to(cr, x + (width - extents.width) / 2, y + (height - extents.height) / 2 - extents.y_bearing);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 4);
	cairo_text_path(cr, text);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
}

static struct egl_surface *
overlay_create(int x, int y, int width, int height)
{
	struct egl_surface *es;
	cairo_surface_t *surface;
	cairo_t *cr;
	int total_width, button_x, button_y;
	const int button_width = 150;
	const int button_height = 40;
	const int spacing = 50;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					     width, height);

	cr = cairo_create(surface);
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.8);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);

	total_width = button_width * 2 + spacing;
	button_x = (width - total_width) / 2;
	button_y = height - button_height - 20;
	draw_button(cr, button_x, button_y, button_width, button_height, "Previous");
	button_x += button_width + spacing;
	draw_button(cr, button_x, button_y, button_width, button_height, "Next");

	cairo_destroy(cr);

	es = egl_surface_create_from_cairo_surface(surface, x, y, width, height);

	cairo_surface_destroy(surface);

	return es;	
}

static void
draw_surface(struct egl_surface *es)
{
	GLint vertices[12];
	GLint tex_coords[12] = { 0, 0,  0, 1,  1, 0,  1, 1 };
	GLuint indices[4] = { 0, 1, 2, 3 };

	vertices[0] = es->map.x;
	vertices[1] = es->map.y;
	vertices[2] = 0;

	vertices[3] = es->map.x;
	vertices[4] = es->map.y + es->map.height;
	vertices[5] = 0;

	vertices[6] = es->map.x + es->map.width;
	vertices[7] = es->map.y;
	vertices[8] = 0;

	vertices[9] = es->map.x + es->map.width;
	vertices[10] = es->map.y + es->map.height;
	vertices[11] = 0;

	glBindTexture(GL_TEXTURE_2D, es->texture);
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

static void
repaint(void *data)
{
	struct egl_compositor *ec = data;
	struct wl_surface_iterator *iterator;
	struct wl_surface *surface;
	struct egl_surface *es;

	draw_surface(ec->background);

	iterator = wl_surface_iterator_create(ec->wl_display, 0);
	while (wl_surface_iterator_next(iterator, &surface)) {
		es = wl_surface_get_data(surface);
		if (es == NULL)
			continue;

		draw_surface(es);
	}
	wl_surface_iterator_destroy(iterator);

	draw_surface(ec->overlay);

	draw_surface(ec->pointer);

	eglSwapBuffers(ec->display, ec->surface);
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
	struct egl_surface *es;

	es = malloc(sizeof *es);
	if (es == NULL)
		return;

	es->surface = EGL_NO_SURFACE;
	wl_surface_set_data(surface, es);

	glGenTextures(1, &es->texture);
}
				   
static void
notify_surface_destroy(struct wl_compositor *compositor,
		       struct wl_surface *surface)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct egl_surface *es;

	es = wl_surface_get_data(surface);
	if (es == NULL)
		return;

	egl_surface_destroy(es, ec);

	schedule_repaint(ec);
}

static void
notify_surface_attach(struct wl_compositor *compositor,
		      struct wl_surface *surface, uint32_t name, 
		      uint32_t width, uint32_t height, uint32_t stride)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct egl_surface *es;

	es = wl_surface_get_data(surface);
	if (es == NULL)
		return;

	if (es->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, es->surface);

	/* FIXME: We need to use a single buffer config without depth
	 * or stencil buffers here to keep egl from creating auxillary
	 * buffers for the pixmap here. */
	es->surface = eglCreateSurfaceForName(ec->display, ec->config,
					      name, width, height, stride, NULL);

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	eglBindTexImage(ec->display, es->surface, GL_TEXTURE_2D);

	schedule_repaint(ec);
}

static void
notify_surface_map(struct wl_compositor *compositor,
		   struct wl_surface *surface, struct wl_map *map)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct egl_surface *es;

	es = wl_surface_get_data(surface);
	if (es == NULL)
		return;

	es->map = *map;

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
	struct egl_surface *es;

	es = wl_surface_get_data(surface);

	/* FIXME: glCopyPixels should work, but then we'll have to
	 * call eglMakeCurrent to set up the src and dest surfaces
	 * first.  This seems cheaper, but maybe there's a better way
	 * to accomplish this. */

	src = eglCreateSurfaceForName(ec->display, ec->config,
				      name, x + width, y + height, stride, NULL);

	eglCopyNativeBuffers(ec->display, es->surface, GL_FRONT_LEFT, dst_x, dst_y,
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

static void
notify_pointer_motion(struct wl_compositor *compositor,
		      struct wl_object *source, int x, int y)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;

	ec->pointer->map.x = x;
	ec->pointer->map.y = y;
	schedule_repaint(ec);
}

static void
notify_key(struct wl_compositor *compositor,
	   struct wl_object *source, uint32_t key, uint32_t state)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;

	if (key == KEY_ESC)
		schedule_repaint(ec);
}

static const struct wl_compositor_interface interface = {
	notify_surface_create,
	notify_surface_destroy,
	notify_surface_attach,
	notify_surface_map,
	notify_surface_copy,
	notify_surface_damage,
	notify_pointer_motion,
	notify_key
};

static const char pointer_device_file[] = 
	"/dev/input/by-id/usb-Apple__Inc._Apple_Internal_Keyboard_._Trackpad-event-mouse";
static const char keyboard_device_file[] = 
	"/dev/input/by-id/usb-Apple__Inc._Apple_Internal_Keyboard_._Trackpad-event-kbd";

static void
create_input_devices(struct wl_display *display)
{
	struct wl_object *obj;
	const char *path;

	path = getenv("WAYLAND_POINTER");
	if (path == NULL)
		path = pointer_device_file;

	obj = wl_input_device_create(display, path);
	if (obj != NULL)
		wl_display_add_object(display, obj);

	path = getenv("WAYLAND_KEYBOARD");
	if (path == NULL)
		path = keyboard_device_file;

	obj = wl_input_device_create(display, path);
	if (obj != NULL)
		wl_display_add_object(display, obj);
}

static const char gem_device[] = "/dev/dri/card0";

WL_EXPORT struct wl_compositor *
wl_compositor_create(struct wl_display *display)
{
	EGLConfig configs[64];
	EGLint major, minor, count;
	struct egl_compositor *ec;
	const char *filename;
	struct screenshooter *shooter;

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

	create_input_devices(display);

	filename = getenv("WAYLAND_BACKGROUND");
	if (filename == NULL)
		filename = "background.jpg";
	ec->background = background_create(filename, 1280, 800);
	ec->pointer = pointer_create(100, 100, 64, 64);
	ec->overlay = overlay_create(0, ec->height - 200, ec->width, 200);

	ec->gem_fd = open(gem_device, O_RDWR);
	if (ec->gem_fd < 0) {
		fprintf(stderr, "failed to open drm device\n");
		return NULL;
	}

	shooter = screenshooter_create(ec);
	wl_display_add_object(display, &shooter->base);
	wl_display_add_global(display, &shooter->base);

	schedule_repaint(ec);

	return &ec->base;
}

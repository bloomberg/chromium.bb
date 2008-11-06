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
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "wayland.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct glx_compositor {
	struct wl_compositor base;
	Display *display;
	GLXContext context;
	Window window;
	struct wl_display *wl_display;
	int gem_fd;
	struct wl_event_source *x_source;
};

struct surface_data {
	GLuint texture;
	struct wl_map map;
};

static void
repaint(void *data)
{
	struct glx_compositor *gc = data;
	struct wl_surface_iterator *iterator;
	struct wl_surface *surface;
	struct surface_data *sd;
	GLint vertices[12];
	GLint tex_coords[12] = { 0, 0,  0, 1,  1, 0,  1, 1 };
	GLuint indices[4] = { 0, 1, 2, 3 };

	iterator = wl_surface_iterator_create(gc->wl_display, 0);
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

	glXSwapBuffers(gc->display, gc->window);
}

static void
schedule_repaint(struct glx_compositor *gc)
{
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(gc->wl_display);
	wl_event_loop_add_idle(loop, repaint, gc);
}

static void
notify_surface_create(struct wl_compositor *compositor,
		      struct wl_surface *surface)
{
	struct surface_data *sd;

	sd = malloc(sizeof *sd);
	if (sd == NULL)
		return;

	wl_surface_set_data(surface, sd);

	glGenTextures(1, &sd->texture);
}
				   
static void
notify_surface_destroy(struct wl_compositor *compositor,
		       struct wl_surface *surface)
{
	struct glx_compositor *gc = (struct glx_compositor *) compositor;
	struct surface_data *sd;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	glDeleteTextures(1, &sd->texture);

	free(sd);

	schedule_repaint(gc);
}

static void
notify_surface_attach(struct wl_compositor *compositor,
		      struct wl_surface *surface, uint32_t name, 
		      uint32_t width, uint32_t height,
		      uint32_t stride)
{
	struct glx_compositor *gc = (struct glx_compositor *) compositor;
	struct surface_data *sd;
	struct drm_gem_open open_arg;
	struct drm_gem_close close_arg;
	struct drm_i915_gem_pread pread;
	uint32_t size;
	void *data;
	int ret;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	open_arg.name = name;
	ret = ioctl(gc->gem_fd, DRM_IOCTL_GEM_OPEN, &open_arg);
	if (ret != 0) {
		fprintf(stderr,
			"failed to gem_open name %d, fd=%d: %m\n",
			name, gc->gem_fd);
		return;
	}

	size = height * stride;
	data = malloc(size);
	if (data == NULL) {
		fprintf(stderr, "malloc for gem_pread failed\n");
		return;
	}

	pread.handle = open_arg.handle;
	pread.pad = 0;
	pread.offset = 0;
	pread.size = size;
	pread.data_ptr = (long) data;

	if (ioctl(gc->gem_fd, DRM_IOCTL_I915_GEM_PREAD, &pread)) {
		fprintf(stderr, "gem_pread failed");
		return;
	}

	close_arg.handle = open_arg.handle;
	ret = ioctl(gc->gem_fd, DRM_IOCTL_GEM_CLOSE, &close_arg);
	if (ret != 0) {
		fprintf(stderr, "failed to gem_close name %d: %m\n", name);
		return;
	}

	glBindTexture(GL_TEXTURE_2D, sd->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		     GL_BGRA, GL_UNSIGNED_BYTE, data);

	free(data);

	schedule_repaint(gc);
}

static void
notify_surface_map(struct wl_compositor *compositor,
		   struct wl_surface *surface, struct wl_map *map)
{
	struct glx_compositor *gc = (struct glx_compositor *) compositor;
	struct surface_data *sd;

	sd = wl_surface_get_data(surface);
	if (sd == NULL)
		return;

	sd->map = *map;

	schedule_repaint(gc);
}

static const struct wl_compositor_interface interface = {
	notify_surface_create,
	notify_surface_destroy,
	notify_surface_attach,
	notify_surface_map
};

static const char gem_device[] = "/dev/dri/card0";

static void
display_data(int fd, uint32_t mask, void *data)
{
	struct glx_compositor *gc = data;
	XEvent ev;

	while (XPending(gc->display) > 0) {
		XNextEvent(gc->display, &ev);
		/* Some day we'll do something useful with these events. */
	}
}

struct wl_compositor *
wl_compositor_create(struct wl_display *display)
{
	static int attribs[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};
	struct glx_compositor *gc;
	const int x = 100, y = 100, width = 1024, height = 768;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;
	int screen;
	struct wl_event_loop *loop;

	gc = malloc(sizeof *gc);
	if (gc == NULL)
		return NULL;

	gc->base.interface = &interface;
	gc->wl_display = display;
	gc->display = XOpenDisplay(NULL);
	if (gc->display == NULL) {
		free(gc);
		return NULL;
	}

	loop = wl_display_get_event_loop(gc->wl_display);
	gc->x_source = wl_event_loop_add_fd(loop,
					    ConnectionNumber(gc->display),
					    WL_EVENT_READABLE,
					    display_data, gc);

	screen = DefaultScreen(gc->display);
	root = RootWindow(gc->display, screen);

	visinfo = glXChooseVisual(gc->display, screen, attribs);

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(gc->display,
					root, visinfo->visual, AllocNone);
	attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
	gc->window = XCreateWindow(gc->display, root, x, y, width, height,
				   0, visinfo->depth, InputOutput,
				   visinfo->visual, mask, &attr);

	gc->context = glXCreateContext(gc->display, visinfo, NULL, True);

	XMapWindow(gc->display, gc->window);
	glXMakeCurrent(gc->display, gc->window, gc->context);

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, 0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glClearColor(0.0, 0.05, 0.2, 0.0);

	gc->gem_fd = open(gem_device, O_RDWR);
	if (gc->gem_fd < 0) {
		fprintf(stderr, "failed to open drm device\n");
		return NULL;
	}

	schedule_repaint(gc);

	return &gc->base;
}

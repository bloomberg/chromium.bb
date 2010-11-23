/*
 * Copyright © 2008-2010 Kristian Høgsberg
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <signal.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/input.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "compositor.h"

struct drm_compositor {
	struct wlsc_compositor base;

	struct udev *udev;
	struct wl_event_source *drm_source;

	struct wl_event_source *term_signal_source;

        /* tty handling state */
	int tty_fd;
	uint32_t vt_active : 1;

	struct termios terminal_attributes;
	struct wl_event_source *tty_input_source;
	struct wl_event_source *enter_vt_source;
	struct wl_event_source *leave_vt_source;
};

struct drm_output {
	struct wlsc_output   base;

	drmModeModeInfo mode;
	uint32_t crtc_id;
	uint32_t connector_id;
	GLuint rbo[2];
	uint32_t fb_id[2];
	EGLImageKHR image[2];
	uint32_t current;	
};

struct drm_input {
	struct wlsc_input_device base;
};

struct evdev_input_device {
	struct drm_input *master;
	struct wl_event_source *source;
	int tool, new_x, new_y;
	int base_x, base_y;
	int fd;
};

static void evdev_input_device_data(int fd, uint32_t mask, void *data)
{
	struct drm_compositor *c;
	struct evdev_input_device *device = data;
	struct input_event ev[8], *e, *end;
	int len, value, dx, dy, absolute_event;
	int x, y;
	uint32_t time;

	c = (struct drm_compositor *) device->master->base.ec;
	if (!c->vt_active)
		return;

	dx = 0;
	dy = 0;
	absolute_event = 0;
	x = device->master->base.x;
	y = device->master->base.y;

	len = read(fd, &ev, sizeof ev);
	if (len < 0 || len % sizeof e[0] != 0) {
		/* FIXME: handle error... reopen device? */;
		return;
	}

	e = ev;
	end = (void *) ev + len;
	for (e = ev; e < end; e++) {
		/* Get the signed value, earlier kernels had this as unsigned */
		value = e->value;
		time = e->time.tv_sec * 1000 + e->time.tv_usec / 1000;

		switch (e->type) {
		case EV_REL:
			switch (e->code) {
			case REL_X:
				dx += value;
				break;

			case REL_Y:
				dy += value;
				break;
			}
			break;

		case EV_ABS:
		        absolute_event = 1;
			switch (e->code) {
			case ABS_X:
				if (device->new_x) {
					device->base_x = x - value;
					device->new_x = 0;
				}
				x = device->base_x + value;
				break;
			case ABS_Y:
				if (device->new_y) {
					device->base_y = y - value;
					device->new_y = 0;
				}
				y = device->base_y + value;
				break;
			}
			break;

		case EV_KEY:
			if (value == 2)
				break;

			switch (e->code) {
			case BTN_TOUCH:
			case BTN_TOOL_PEN:
			case BTN_TOOL_RUBBER:
			case BTN_TOOL_BRUSH:
			case BTN_TOOL_PENCIL:
			case BTN_TOOL_AIRBRUSH:
			case BTN_TOOL_FINGER:
			case BTN_TOOL_MOUSE:
			case BTN_TOOL_LENS:
				if (device->tool == 0 && value) {
					device->new_x = 1;
					device->new_y = 1;
				}
				device->tool = value ? e->code : 0;
				break;

			case BTN_LEFT:
			case BTN_RIGHT:
			case BTN_MIDDLE:
			case BTN_SIDE:
			case BTN_EXTRA:
			case BTN_FORWARD:
			case BTN_BACK:
			case BTN_TASK:
				notify_button(&device->master->base,
					      time, e->code, value);
				break;

			default:
				notify_key(&device->master->base,
					   time, e->code, value);
				break;
			}
		}
	}

	if (dx != 0 || dy != 0)
		notify_motion(&device->master->base, time, x + dx, y + dy);
	if (absolute_event && device->tool)
		notify_motion(&device->master->base, time, x, y);
}

static struct evdev_input_device *
evdev_input_device_create(struct drm_input *master,
			  struct wl_display *display, const char *path)
{
	struct evdev_input_device *device;
	struct wl_event_loop *loop;

	device = malloc(sizeof *device);
	if (device == NULL)
		return NULL;

	device->tool = 1;
	device->new_x = 1;
	device->new_y = 1;
	device->master = master;

	device->fd = open(path, O_RDONLY);
	if (device->fd < 0) {
		free(device);
		fprintf(stderr, "couldn't create pointer for %s: %m\n", path);
		return NULL;
	}

	loop = wl_display_get_event_loop(display);
	device->source = wl_event_loop_add_fd(loop, device->fd,
					      WL_EVENT_READABLE,
					      evdev_input_device_data, device);
	if (device->source == NULL) {
		close(device->fd);
		free(device);
		return NULL;
	}

	return device;
}

static void
drm_input_create(struct drm_compositor *c)
{
	struct drm_input *input;
	struct udev_enumerate *e;
        struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);
	wlsc_input_device_init(&input->base, &c->base);

	e = udev_enumerate_new(c->udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_add_match_property(e, "WAYLAND_SEAT", "1");
        udev_enumerate_scan_devices(e);
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(c->udev, path);
		evdev_input_device_create(input, c->base.wl_display,
					  udev_device_get_devnode(device));
	}
        udev_enumerate_unref(e);

	c->base.input_device = &input->base;
}

static void
drm_compositor_present(struct wlsc_compositor *ec)
{
	struct drm_compositor *c = (struct drm_compositor *) ec;
	struct drm_output *output;

	wl_list_for_each(output, &ec->output_list, base.link) {
		output->current ^= 1;

		glFramebufferRenderbuffer(GL_FRAMEBUFFER,
					  GL_COLOR_ATTACHMENT0,
					  GL_RENDERBUFFER,
					  output->rbo[output->current]);

		drmModePageFlip(c->base.drm.fd, output->crtc_id,
				output->fb_id[output->current ^ 1],
				DRM_MODE_PAGE_FLIP_EVENT, output);
	}	
}

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct wlsc_output *output = data;
	struct wlsc_compositor *compositor = output->compositor;
	uint32_t msecs;

	msecs = sec * 1000 + usec / 1000;
	wlsc_compositor_finish_frame(compositor, msecs);
}

static void
on_drm_input(int fd, uint32_t mask, void *data)
{
	drmEventContext evctx;

	memset(&evctx, 0, sizeof evctx);
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.page_flip_handler = page_flip_handler;
	drmHandleEvent(fd, &evctx);
}

static int
init_egl(struct drm_compositor *ec, struct udev_device *device)
{
	EGLint major, minor;
	const char *extensions, *filename;
	int fd;
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	filename = udev_device_get_devnode(device);
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		/* Probably permissions error */
		fprintf(stderr, "couldn't open %s, skipping\n",
			udev_device_get_devnode(device));
		return -1;
	}

	wlsc_drm_init(&ec->base, fd, filename);

	ec->base.display = eglGetDRMDisplayMESA(ec->base.drm.fd);
	if (ec->base.display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return -1;
	}

	if (!eglInitialize(ec->base.display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return -1;
	}

	extensions = eglQueryString(ec->base.display, EGL_EXTENSIONS);
	if (!strstr(extensions, "EGL_KHR_surfaceless_opengl")) {
		fprintf(stderr, "EGL_KHR_surfaceless_opengl not available\n");
		return -1;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	ec->base.context = eglCreateContext(ec->base.display, NULL,
					    EGL_NO_CONTEXT, context_attribs);
	if (ec->base.context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return -1;
	}

	if (!eglMakeCurrent(ec->base.display, EGL_NO_SURFACE,
			    EGL_NO_SURFACE, ec->base.context)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}

	return 0;
}

static drmModeModeInfo builtin_1024x768 = {
	63500,			/* clock */
	1024, 1072, 1176, 1328, 0,
	768, 771, 775, 798, 0,
	59920,
	DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
	0,
	"1024x768"
};

static int
create_output_for_connector(struct drm_compositor *ec,
			    drmModeRes *resources,
			    drmModeConnector *connector)
{
	struct drm_output *output;
	drmModeEncoder *encoder;
	drmModeModeInfo *mode;
	int i, ret;
	EGLint handle, stride, attribs[] = {
		EGL_WIDTH,		0,
		EGL_HEIGHT,		0,
		EGL_DRM_BUFFER_FORMAT_MESA,	EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA,	EGL_DRM_BUFFER_USE_SCANOUT_MESA,
		EGL_NONE
	};

	output = malloc(sizeof *output);
	if (output == NULL)
		return -1;

	if (connector->count_modes > 0) 
		mode = &connector->modes[0];
	else
		mode = &builtin_1024x768;

	encoder = drmModeGetEncoder(ec->base.drm.fd, connector->encoders[0]);
	if (encoder == NULL) {
		fprintf(stderr, "No encoder for connector.\n");
		return -1;
	}

	for (i = 0; i < resources->count_crtcs; i++) {
		if (encoder->possible_crtcs & (1 << i))
			break;
	}
	if (i == resources->count_crtcs) {
		fprintf(stderr, "No usable crtc for encoder.\n");
		return -1;
	}

	memset(output, 0, sizeof *output);
	wlsc_output_init(&output->base, &ec->base, 0, 0,
			 mode->hdisplay, mode->vdisplay);

	output->crtc_id = resources->crtcs[i];
	output->connector_id = connector->connector_id;
	output->mode = *mode;

	drmModeFreeEncoder(encoder);

	glGenRenderbuffers(2, output->rbo);
	for (i = 0; i < 2; i++) {
		glBindRenderbuffer(GL_RENDERBUFFER, output->rbo[i]);

		attribs[1] = output->base.width;
		attribs[3] = output->base.height;
		output->image[i] =
			eglCreateDRMImageMESA(ec->base.display, attribs);
		glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
						       output->image[i]);
		eglExportDRMImageMESA(ec->base.display, output->image[i],
				      NULL, &handle, &stride);

		ret = drmModeAddFB(ec->base.drm.fd,
				   output->base.width, output->base.height,
				   32, 32, stride, handle, &output->fb_id[i]);
		if (ret) {
			fprintf(stderr, "failed to add fb %d: %m\n", i);
			return -1;
		}
	}

	output->current = 0;
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  output->rbo[output->current]);
	ret = drmModeSetCrtc(ec->base.drm.fd, output->crtc_id,
			     output->fb_id[output->current ^ 1], 0, 0,
			     &output->connector_id, 1, &output->mode);
	if (ret) {
		fprintf(stderr, "failed to set mode: %m\n");
		return -1;
	}

	wl_list_insert(ec->base.output_list.prev, &output->base.link);

	return 0;
}

static int
create_outputs(struct drm_compositor *ec, int option_connector)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	int i;

	resources = drmModeGetResources(ec->base.drm.fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return -1;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(ec->base.drm.fd, resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		    (option_connector == 0 ||
		     connector->connector_id == option_connector))
			if (create_output_for_connector(ec, resources, connector) < 0)
				return -1;

		drmModeFreeConnector(connector);
	}

	if (wl_list_empty(&ec->base.output_list)) {
		fprintf(stderr, "No currently active connector found.\n");
		return -1;
	}

	drmModeFreeResources(resources);

	return 0;
}

static void on_enter_vt(int signal_number, void *data)
{
	struct drm_compositor *ec = data;
	struct drm_output *output;
	int ret;

	ret = drmSetMaster(ec->base.drm.fd);
	if (ret) {
		fprintf(stderr, "failed to set drm master\n");
		kill(0, SIGTERM);
		return;
	}

	fprintf(stderr, "enter vt\n");

	ioctl(ec->tty_fd, VT_RELDISP, VT_ACKACQ);
	ret = ioctl(ec->tty_fd, KDSETMODE, KD_GRAPHICS);
	if (ret)
		fprintf(stderr, "failed to set KD_GRAPHICS mode on console: %m\n");
	ec->vt_active = 1;

	wl_list_for_each(output, &ec->base.output_list, base.link) {
		ret = drmModeSetCrtc(ec->base.drm.fd, output->crtc_id,
				     output->fb_id[output->current ^ 1], 0, 0,
				     &output->connector_id, 1, &output->mode);
		if (ret)
			fprintf(stderr,
				"failed to set mode for connector %d: %m\n",
				output->connector_id);
	}
}

static void on_leave_vt(int signal_number, void *data)
{
	struct drm_compositor *ec = data;
	int ret;

	ret = drmDropMaster(ec->base.drm.fd);
	if (ret) {
		fprintf(stderr, "failed to drop drm master\n");
		kill(0, SIGTERM);
		return;
	}

	ioctl (ec->tty_fd, VT_RELDISP, 1);
	ret = ioctl(ec->tty_fd, KDSETMODE, KD_TEXT);
	if (ret)
		fprintf(stderr, "failed to set KD_TEXT mode on console: %m\n");
	ec->vt_active = 0;
}

static void
on_tty_input(int fd, uint32_t mask, void *data)
{
	struct drm_compositor *ec = data;

	/* Ignore input to tty.  We get keyboard events from evdev
	 */
	tcflush(ec->tty_fd, TCIFLUSH);
}

static void on_term_signal(int signal_number, void *data)
{
	struct drm_compositor *ec = data;

	if (tcsetattr(ec->tty_fd, TCSANOW, &ec->terminal_attributes) < 0)
		fprintf(stderr, "could not restore terminal to canonical mode\n");

	exit(0);
}

static int setup_tty(struct drm_compositor *ec, struct wl_event_loop *loop)
{
	struct termios raw_attributes;
	struct vt_mode mode = { 0 };
	int ret;

	ec->tty_fd = open("/dev/tty0", O_RDWR | O_NOCTTY);
	if (ec->tty_fd <= 0) {
		fprintf(stderr, "failed to open active tty: %m\n");
		return -1;
	}

	if (tcgetattr(ec->tty_fd, &ec->terminal_attributes) < 0) {
		fprintf(stderr, "could not get terminal attributes: %m\n");
		return -1;
	}

	/* Ignore control characters and disable echo */
	raw_attributes = ec->terminal_attributes;
	cfmakeraw(&raw_attributes);

	/* Fix up line endings to be normal (cfmakeraw hoses them) */
	raw_attributes.c_oflag |= OPOST | OCRNL;

	if (tcsetattr(ec->tty_fd, TCSANOW, &raw_attributes) < 0)
		fprintf(stderr, "could not put terminal into raw mode: %m\n");

	ec->term_signal_source =
		wl_event_loop_add_signal(loop, SIGTERM, on_term_signal, ec);

	ec->tty_input_source =
		wl_event_loop_add_fd(loop, ec->tty_fd,
				     WL_EVENT_READABLE, on_tty_input, ec);

	ret = ioctl(ec->tty_fd, KDSETMODE, KD_GRAPHICS);
	if (ret)
		fprintf(stderr, "failed to set KD_GRAPHICS mode on tty: %m\n");

	ec->vt_active = 1;
	mode.mode = VT_PROCESS;
	mode.relsig = SIGUSR1;
	mode.acqsig = SIGUSR2;
	if (!ioctl(ec->tty_fd, VT_SETMODE, &mode) < 0) {
		fprintf(stderr, "failed to take control of vt handling\n");
	}

	ec->leave_vt_source =
		wl_event_loop_add_signal(loop, SIGUSR1, on_leave_vt, ec);
	ec->enter_vt_source =
		wl_event_loop_add_signal(loop, SIGUSR2, on_enter_vt, ec);

	return 0;
}

static int
drm_authenticate(struct wlsc_compositor *c, uint32_t id)
{
	struct drm_compositor *ec = (struct drm_compositor *) c;

	return drmAuthMagic(ec->base.drm.fd, id);
}

struct wlsc_compositor *
drm_compositor_create(struct wl_display *display, int connector)
{
	struct drm_compositor *ec;
	struct udev_enumerate *e;
        struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path;
	struct wl_event_loop *loop;

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	memset(ec, 0, sizeof *ec);
	ec->udev = udev_new();
	if (ec->udev == NULL) {
		fprintf(stderr, "failed to initialize udev context\n");
		return NULL;
	}

	e = udev_enumerate_new(ec->udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_property(e, "WAYLAND_SEAT", "1");
        udev_enumerate_scan_devices(e);
	device = NULL;
        udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(ec->udev, path);
		break;
	}
        udev_enumerate_unref(e);

	if (device == NULL) {
		fprintf(stderr, "no drm device found\n");
		return NULL;
	}

	ec->base.wl_display = display;
	if (init_egl(ec, device) < 0) {
		fprintf(stderr, "failed to initialize egl\n");
		return NULL;
	}
	
	/* Can't init base class until we have a current egl context */
	if (wlsc_compositor_init(&ec->base, display) < 0)
		return NULL;

	if (create_outputs(ec, connector) < 0) {
		fprintf(stderr, "failed to create output for %s\n", path);
		return NULL;
	}

	drm_input_create(ec);

	loop = wl_display_get_event_loop(ec->base.wl_display);
	ec->drm_source =
		wl_event_loop_add_fd(loop, ec->base.drm.fd,
				     WL_EVENT_READABLE, on_drm_input, ec);
	setup_tty(ec, loop);
	ec->base.authenticate = drm_authenticate;
	ec->base.present = drm_compositor_present;
	ec->base.focus = 1;

	return &ec->base;
}

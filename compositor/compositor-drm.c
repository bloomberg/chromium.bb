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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "compositor.h"

struct drm_compositor {
	struct wlsc_compositor base;

	struct udev *udev;
	struct wl_event_source *drm_source;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_drm_source;

	struct {
		int fd;
	} drm;
	uint32_t crtc_allocator;
	uint32_t connector_allocator;
	struct tty *tty;

	PFNEGLCREATEDRMIMAGEMESA create_drm_image;
	PFNEGLEXPORTDRMIMAGEMESA export_drm_image;
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

	struct wlsc_surface *scanout_surface;

	uint32_t fs_surf_fb_id;
	uint32_t pending_fs_surf_fb_id;
};

static int
drm_output_prepare_render(struct wlsc_output *output_base)
{
	struct drm_output *output = (struct drm_output *) output_base;

	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  output->rbo[output->current]);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return -1;

	return 0;
}

static int
drm_output_present(struct wlsc_output *output_base)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	uint32_t fb_id = 0;

	if (drm_output_prepare_render(&output->base))
		return -1;
	glFlush();

	output->current ^= 1;

	if (output->scanout_surface != NULL) {
		output->scanout_surface = NULL;
		fb_id = output->fs_surf_fb_id;
	} else {
		fb_id = output->fb_id[output->current ^ 1];
	}

	drmModePageFlip(c->drm.fd, output->crtc_id,
			fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, output);

	return 0;
}

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct drm_output *output = (struct drm_output *) data;
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	uint32_t msecs;

	if (output->pending_fs_surf_fb_id) {
		drmModeRmFB(c->drm.fd, output->pending_fs_surf_fb_id);
		output->pending_fs_surf_fb_id = 0;
	}

	if (output->fs_surf_fb_id) {
		output->pending_fs_surf_fb_id = output->fs_surf_fb_id;
		output->fs_surf_fb_id = 0;
	}

	msecs = sec * 1000 + usec / 1000;
	wlsc_output_finish_frame(&output->base, msecs);
}

static int
drm_output_prepare_scanout_surface(struct wlsc_output *output_base,
				   struct wlsc_surface *es)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	EGLint handle, stride;
	int ret;
	uint32_t fb_id = 0;

	if (es->x != output->base.x ||
	    es->y != output->base.y ||
	    es->width != output->base.width ||
	    es->height != output->base.height ||
	    es->image == EGL_NO_IMAGE_KHR)
		return -1;

	c->export_drm_image(c->base.display,
			    es->image, NULL, &handle, &stride);

	if (handle == 0)
		return -1;

	ret = drmModeAddFB(c->drm.fd,
			   output->base.width, output->base.height,
			   32, 32, stride, handle, &fb_id);

	if (ret)
		return -1;

	output->fs_surf_fb_id = fb_id;
	output->scanout_surface = es;

	return 0;
}

static int
drm_output_set_cursor(struct wlsc_output *output_base,
		      struct wlsc_input_device *eid)
{
	struct drm_output *output = (struct drm_output *) output_base;
	struct drm_compositor *c =
		(struct drm_compositor *) output->base.compositor;
	EGLint handle, stride;
	int ret = -1;
	pixman_region32_t cursor_region;

	if (eid == NULL) {
		drmModeSetCursor(c->drm.fd, output->crtc_id, 0, 0, 0);
		return 0;
	}

	pixman_region32_init_rect(&cursor_region,
				  eid->sprite->x, eid->sprite->y,
				  eid->sprite->width, eid->sprite->height);

	pixman_region32_intersect_rect(&cursor_region, &cursor_region,
				       output->base.x, output->base.y,
				       output->base.width, output->base.height);

	if (!pixman_region32_not_empty(&cursor_region)) {
		ret = 0;
		goto out;
	}

	if (eid->sprite->image == EGL_NO_IMAGE_KHR)
		goto out;

	if (eid->sprite->width > 64 || eid->sprite->height > 64)
		goto out;
	
	c->export_drm_image(c->base.display, eid->sprite->image,
			    NULL, &handle, &stride);

	if (stride != 64 * 4) {
		fprintf(stderr, "info: cursor stride is != 64\n");
		goto out;
	}

	ret = drmModeSetCursor(c->drm.fd, output->crtc_id, handle, 64, 64);
	if (ret) {
		fprintf(stderr, "failed to set cursor: %s\n", strerror(-ret));
		goto out;
	}

	ret = drmModeMoveCursor(c->drm.fd, output->crtc_id,
				eid->sprite->x - output->base.x,
				eid->sprite->y - output->base.y);
	if (ret) {
		fprintf(stderr, "failed to move cursor: %s\n", strerror(-ret));
		goto out;
	}

	printf("info: set hardware cursor\n");

out:
	pixman_region32_fini(&cursor_region);
	if (ret)
		drmModeSetCursor(c->drm.fd, output->crtc_id, 0, 0, 0);
	return ret;
}

static int
on_drm_input(int fd, uint32_t mask, void *data)
{
	drmEventContext evctx;

	memset(&evctx, 0, sizeof evctx);
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.page_flip_handler = page_flip_handler;
	drmHandleEvent(fd, &evctx);

	return 1;
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
	fd = open(filename, O_RDWR, O_CLOEXEC);
	if (fd < 0) {
		/* Probably permissions error */
		fprintf(stderr, "couldn't open %s, skipping\n",
			udev_device_get_devnode(device));
		return -1;
	}

	setenv("EGL_PLATFORM", "drm", 1);
	ec->drm.fd = fd;
	ec->base.display = eglGetDisplay(FD_TO_EGL_NATIVE_DPY(ec->drm.fd));
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
			    drmModeConnector *connector,
			    int x, int y)
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

	encoder = drmModeGetEncoder(ec->drm.fd, connector->encoders[0]);
	if (encoder == NULL) {
		fprintf(stderr, "No encoder for connector.\n");
		return -1;
	}

	for (i = 0; i < resources->count_crtcs; i++) {
		if (encoder->possible_crtcs & (1 << i) &&
		    !(ec->crtc_allocator & (1 << resources->crtcs[i])))
			break;
	}
	if (i == resources->count_crtcs) {
		fprintf(stderr, "No usable crtc for encoder.\n");
		return -1;
	}

	memset(output, 0, sizeof *output);
	wlsc_output_init(&output->base, &ec->base, x, y,
			 mode->hdisplay, mode->vdisplay, 0);

	output->crtc_id = resources->crtcs[i];
	ec->crtc_allocator |= (1 << output->crtc_id);

	output->connector_id = connector->connector_id;
	ec->connector_allocator |= (1 << output->connector_id);
	output->mode = *mode;

	drmModeFreeEncoder(encoder);

	glGenRenderbuffers(2, output->rbo);
	for (i = 0; i < 2; i++) {
		glBindRenderbuffer(GL_RENDERBUFFER, output->rbo[i]);

		attribs[1] = output->base.width;
		attribs[3] = output->base.height;
		output->image[i] =
			ec->create_drm_image(ec->base.display, attribs);
		ec->base.image_target_renderbuffer_storage(GL_RENDERBUFFER,
							   output->image[i]);
		ec->export_drm_image(ec->base.display, output->image[i],
				     NULL, &handle, &stride);

		ret = drmModeAddFB(ec->drm.fd,
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
	ret = drmModeSetCrtc(ec->drm.fd, output->crtc_id,
			     output->fb_id[output->current ^ 1], 0, 0,
			     &output->connector_id, 1, &output->mode);
	if (ret) {
		fprintf(stderr, "failed to set mode: %m\n");
		return -1;
	}

	output->scanout_surface = NULL;
	output->base.prepare_render = drm_output_prepare_render;
	output->base.present = drm_output_present;
	output->base.prepare_scanout_surface =
		drm_output_prepare_scanout_surface;
	output->base.set_hardware_cursor = drm_output_set_cursor;

	wl_list_insert(ec->base.output_list.prev, &output->base.link);

	return 0;
}

static int
create_outputs(struct drm_compositor *ec, int option_connector)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	int i;
	int x = 0, y = 0;

	resources = drmModeGetResources(ec->drm.fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return -1;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(ec->drm.fd, resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		    (option_connector == 0 ||
		     connector->connector_id == option_connector))
			if (create_output_for_connector(ec, resources,
							connector, x, y) < 0)
				return -1;

		x += container_of(ec->base.output_list.prev, struct wlsc_output,
				  link)->width;

		drmModeFreeConnector(connector);
	}

	if (wl_list_empty(&ec->base.output_list)) {
		fprintf(stderr, "No currently active connector found.\n");
		return -1;
	}

	drmModeFreeResources(resources);

	return 0;
}

static int
destroy_output(struct drm_output *output)
{
	struct drm_compositor *ec =
		(struct drm_compositor *) output->base.compositor;
	int i;

	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  0);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glDeleteRenderbuffers(2, output->rbo);

	for (i = 0; i < 2; i++) {
		ec->base.destroy_image(ec->base.display, output->image[i]);
		drmModeRmFB(ec->drm.fd, output->fb_id[i]);
	}
	
	ec->crtc_allocator &= ~(1 << output->crtc_id);
	ec->connector_allocator &= ~(1 << output->connector_id);

	wlsc_output_destroy(&output->base);
	wl_list_remove(&output->base.link);

	free(output);

	return 0;
}

static void
update_outputs(struct drm_compositor *ec)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	struct drm_output *output, *next;
	int x = 0, y = 0;
	int x_offset = 0, y_offset = 0;
	uint32_t connected = 0, disconnects = 0;
	int i;

	resources = drmModeGetResources(ec->drm.fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return;
	}

	/* collect new connects */
	for (i = 0; i < resources->count_connectors; i++) {
		connector =
			drmModeGetConnector(ec->drm.fd,
					    resources->connectors[i]);
		if (connector == NULL ||
		    connector->connection != DRM_MODE_CONNECTED)
			continue;

		connected |= (1 << connector->connector_id);
		
		if (!(ec->connector_allocator & (1 << connector->connector_id))) {
			struct wlsc_output *last_output =
				container_of(ec->base.output_list.prev,
					     struct wlsc_output, link);

			/* XXX: not yet needed, we die with 0 outputs */
			if (!wl_list_empty(&ec->base.output_list))
				x = last_output->x + last_output->width;
			else
				x = 0;
			y = 0;
			create_output_for_connector(ec, resources,
						    connector, x, y);
			printf("connector %d connected\n",
			       connector->connector_id);
				
		}
		drmModeFreeConnector(connector);
	}
	drmModeFreeResources(resources);

	disconnects = ec->connector_allocator & ~connected;
	if (disconnects) {
		wl_list_for_each_safe(output, next, &ec->base.output_list,
				      base.link) {
			if (x_offset != 0 || y_offset != 0) {
				wlsc_output_move(&output->base,
						 output->base.x - x_offset,
						 output->base.y - y_offset);
			}

			if (disconnects & (1 << output->connector_id)) {
				disconnects &= ~(1 << output->connector_id);
				printf("connector %d disconnected\n",
				       output->connector_id);
				x_offset += output->base.width;
				destroy_output(output);
			}
		}
	}

	/* FIXME: handle zero outputs, without terminating */	
	if (ec->connector_allocator == 0)
		wl_display_terminate(ec->base.wl_display);
}

static int
udev_event_is_hotplug(struct udev_device *device)
{
	struct udev_list_entry *list, *hotplug_entry;
	
	list = udev_device_get_properties_list_entry(device);

	hotplug_entry = udev_list_entry_get_by_name(list, "HOTPLUG");
	if (hotplug_entry == NULL)
		return 0;

	return strcmp(udev_list_entry_get_value(hotplug_entry), "1") == 0;
}

static int
udev_drm_event(int fd, uint32_t mask, void *data)
{
	struct drm_compositor *ec = data;
	struct udev_device *event;

	event = udev_monitor_receive_device(ec->udev_monitor);
	
	if (udev_event_is_hotplug(event))
		update_outputs(ec);

	udev_device_unref(event);

	return 1;
}

static EGLImageKHR
drm_compositor_create_cursor_image(struct wlsc_compositor *ec,
				   int32_t width, int32_t height)
{
	static const EGLint image_attribs[] = {
		EGL_WIDTH, 64,
		EGL_HEIGHT, 64,
		EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA, EGL_DRM_BUFFER_USE_CURSOR_MESA,
		EGL_NONE
	};
	struct drm_compositor *c = (struct drm_compositor *) ec;

	if (width > 64 || height > 64)
		return EGL_NO_IMAGE_KHR;

	return c->create_drm_image(ec->display, image_attribs);
}

static void
drm_destroy(struct wlsc_compositor *ec)
{
	struct drm_compositor *d = (struct drm_compositor *) ec;

	tty_destroy(d->tty);

	free(d);
}

static void
vt_func(struct wlsc_compositor *compositor, int event)
{
	struct drm_compositor *ec = (struct drm_compositor *) compositor;
	struct wlsc_output *output;

	switch (event) {
	case TTY_ENTER_VT:
		compositor->focus = 1;
		drmSetMaster(ec->drm.fd);
		compositor->state = WLSC_COMPOSITOR_ACTIVE;
		wlsc_compositor_damage_all(compositor);
		break;
	case TTY_LEAVE_VT:
		compositor->focus = 0;
		compositor->state = WLSC_COMPOSITOR_SLEEPING;
		drmDropMaster(ec->drm.fd);

		wl_list_for_each(output, &ec->base.output_list, link)
			drm_output_set_cursor(output, NULL);

		break;
	};
}

static struct wlsc_compositor *
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

	ec->base.destroy = drm_destroy;
	ec->base.create_cursor_image = drm_compositor_create_cursor_image;

	ec->base.focus = 1;

	glGenFramebuffers(1, &ec->base.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, ec->base.fbo);

	ec->create_drm_image =
		(void *) eglGetProcAddress("eglCreateDRMImageMESA");
	ec->export_drm_image =
		(void *) eglGetProcAddress("eglExportDRMImageMESA");

	/* Can't init base class until we have a current egl context */
	if (wlsc_compositor_init(&ec->base, display) < 0)
		return NULL;

	if (create_outputs(ec, connector) < 0) {
		fprintf(stderr, "failed to create output for %s\n", path);
		return NULL;
	}

	evdev_input_add_devices(&ec->base, ec->udev);

	loop = wl_display_get_event_loop(ec->base.wl_display);
	ec->drm_source =
		wl_event_loop_add_fd(loop, ec->drm.fd,
				     WL_EVENT_READABLE, on_drm_input, ec);
	ec->tty = tty_create(&ec->base, vt_func);

	ec->udev_monitor = udev_monitor_new_from_netlink(ec->udev, "udev");
	if (ec->udev_monitor == NULL) {
		fprintf(stderr, "failed to intialize udev monitor\n");
		return NULL;
	}
	udev_monitor_filter_add_match_subsystem_devtype(ec->udev_monitor,
							"drm", NULL);
	ec->udev_drm_source =
		wl_event_loop_add_fd(loop, udev_monitor_get_fd(ec->udev_monitor),
				     WL_EVENT_READABLE, udev_drm_event, ec);

	if (udev_monitor_enable_receiving(ec->udev_monitor) < 0) {
		fprintf(stderr, "failed to enable udev-monitor receiving\n");
		return NULL;
	}

	return &ec->base;
}

struct wlsc_compositor *
backend_init(struct wl_display *display, char *options);

WL_EXPORT struct wlsc_compositor *
backend_init(struct wl_display *display, char *options)
{
	int connector = 0, i;
	char *p, *value;

	static char * const tokens[] = { "connector", NULL };
	
	p = options;
	while (i = getsubopt(&p, tokens, &value), i != -1) {
		switch (i) {
		case 0:
			connector = strtol(value, NULL, 0);
			break;
		}
	}

	return drm_compositor_create(display, connector);
}

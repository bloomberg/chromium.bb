/*
 * Copyright © 2011 Benjamin Franzke
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

#include <sys/time.h>

#include <WF/wfd.h>
#include <WF/wfdext.h>

#include <gbm.h>

#include "compositor.h"

struct wfd_compositor {
	struct wlsc_compositor base;

	struct udev *udev;
	struct gbm_device *gbm;
	WFDDevice dev;

	WFDEvent event;
	int wfd_fd;
	struct wl_event_source *wfd_source;

	struct tty *tty;

	uint32_t start_time;
	uint32_t used_pipelines;
};

struct wfd_mode {
	struct wlsc_mode base;
	WFDPortMode mode;
};

struct wfd_output {
	struct wlsc_output   base;

	WFDPort port;

	WFDPipeline pipeline;
	WFDint pipeline_id;

	WFDPortMode mode;
	WFDSource source[2];

	struct gbm_bo *bo[2];
	EGLImageKHR image[2];
	GLuint rbo[2];
	uint32_t current;
};

union wfd_geometry {
	struct {
		WFDint x, y;
		WFDint width, height;
	} g;

	WFDint array[4];
};

static int
wfd_output_prepare_render(struct wlsc_output *output_base)
{
	struct wfd_output *output = (struct wfd_output *) output_base;

	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  output->rbo[output->current]);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return -1;

	return 0;
}

static int
wfd_output_present(struct wlsc_output *output_base)
{
	struct wfd_output *output = (struct wfd_output *) output_base;
	struct wfd_compositor *c =
		(struct wfd_compositor *) output->base.compositor;

	if (wfd_output_prepare_render(&output->base))
		return -1;
	glFlush();

	output->current ^= 1;

	wfdBindSourceToPipeline(c->dev, output->pipeline,
				output->source[output->current ^ 1],
				WFD_TRANSITION_AT_VSYNC, NULL);

	wfdDeviceCommit(c->dev, WFD_COMMIT_PIPELINE, output->pipeline);

	return 0;
}

static int
init_egl(struct wfd_compositor *ec)
{
	EGLint major, minor;
	const char *extensions;
	int fd;
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	fd = wfdGetDeviceAttribi(ec->dev, WFD_DEVICE_ID);
	if (fd < 0)
		return -1;

	ec->wfd_fd = fd;
	ec->gbm = gbm_create_device(ec->wfd_fd);
	ec->base.display = eglGetDisplay(ec->gbm);
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

static int
wfd_output_prepare_scanout_surface(struct wlsc_output *output_base,
				   struct wlsc_surface *es)
{
	return -1;
}

static int
wfd_output_set_cursor(struct wlsc_output *output_base,
		      struct wlsc_input_device *input)
{
	return -1;
}

static void
wfd_output_destroy(struct wlsc_output *output_base)
{
	struct wfd_output *output = (struct wfd_output *) output_base;
	struct wfd_compositor *ec =
		(struct wfd_compositor *) output->base.compositor;
	int i;

	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  0);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glDeleteRenderbuffers(2, output->rbo);

	for (i = 0; i < 2; i++) {
		ec->base.destroy_image(ec->base.display, output->image[i]);
		gbm_bo_destroy(output->bo[i]);
		wfdDestroySource(ec->dev, output->source[i]);
	}
	
	ec->used_pipelines &= ~(1 << output->pipeline_id);
	wfdDestroyPipeline(ec->dev, output->pipeline);
	wfdDestroyPort(ec->dev, output->port);

	wlsc_output_destroy(&output->base);
	wl_list_remove(&output->base.link);

	free(output);
}

static int
wfd_output_add_mode(struct wfd_output *output, WFDPortMode mode)
{
	struct wfd_compositor *ec =
		(struct wfd_compositor *) output->base.compositor;
	struct wfd_mode *wmode;

	wmode = malloc(sizeof *wmode);
	if (wmode == NULL)
		return -1;

	wmode->base.flags = 0;
	wmode->base.width = wfdGetPortModeAttribi(ec->dev, output->port, mode,
						  WFD_PORT_MODE_WIDTH);
	wmode->base.height = wfdGetPortModeAttribi(ec->dev, output->port, mode,
						   WFD_PORT_MODE_HEIGHT);
	wmode->base.refresh = wfdGetPortModeAttribi(ec->dev, output->port, mode,
						    WFD_PORT_MODE_REFRESH_RATE);
	wmode->mode = mode;
	wl_list_insert(output->base.mode_list.prev, &wmode->base.link);

	return 0;
}

static int
create_output_for_port(struct wfd_compositor *ec,
		       WFDHandle port,
		       int x, int y)
{
	struct wfd_output *output;
	int i;
	WFDint num_pipelines, *pipelines;
	WFDint num_modes;
	union wfd_geometry geometry;
	struct wfd_mode *mode;
	WFDPortMode *modes;
	WFDfloat physical_size[2];

	output = malloc(sizeof *output);
	if (output == NULL)
		return -1;

	memset(output, 0, sizeof *output);
	memset(&geometry, 0, sizeof geometry);

	output->port = port;
	wl_list_init(&output->base.mode_list);

	wfdSetPortAttribi(ec->dev, output->port,
			  WFD_PORT_POWER_MODE, WFD_POWER_MODE_ON);

	num_modes = wfdGetPortModes(ec->dev, output->port, NULL, 0);
	if (num_modes < 1) {
		fprintf(stderr, "failed to get port mode\n");
		goto cleanup_port;
	}

	modes = malloc(sizeof(WFDPortMode) * num_modes);
	if (modes == NULL) 
		goto cleanup_port;

	output->base.compositor = &ec->base;
	num_modes = wfdGetPortModes(ec->dev, output->port, modes, num_modes);
	for (i = 0; i < num_modes; ++i)
		wfd_output_add_mode(output, modes[i]);

	free(modes);

	wfdGetPortAttribiv(ec->dev, output->port,
			   WFD_PORT_NATIVE_RESOLUTION,
			   2, &geometry.array[2]);

	output->base.current = NULL;
	wl_list_for_each(mode, &output->base.mode_list, base.link) {
		if (mode->base.width == geometry.g.width &&
		    mode->base.height == geometry.g.height) {
			output->base.current = &mode->base;
			break;
		}
	}
	if (output->base.current == NULL) {
		fprintf(stderr, "failed to find a native mode\n");
		goto cleanup_port;
	}

	mode = (struct wfd_mode *) output->base.current;
	mode->base.flags = WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

	wfdSetPortMode(ec->dev, output->port, mode->mode);

	wfdEnumeratePipelines(ec->dev, NULL, 0, NULL);

	num_pipelines = wfdGetPortAttribi(ec->dev, output->port,
					  WFD_PORT_PIPELINE_ID_COUNT);
	if (num_pipelines < 1) {
		fprintf(stderr, "failed to get a bindable pipeline\n");
		goto cleanup_port;
	}
	pipelines = calloc(num_pipelines, sizeof *pipelines);
	if (pipelines == NULL)
		goto cleanup_port;

	wfdGetPortAttribiv(ec->dev, output->port,
			   WFD_PORT_BINDABLE_PIPELINE_IDS,
			   num_pipelines, pipelines);

	output->pipeline_id = WFD_INVALID_PIPELINE_ID;
	for (i = 0; i < num_pipelines; ++i) {
		if (!(ec->used_pipelines & (1 << pipelines[i]))) {
			output->pipeline_id = pipelines[i];
			break;
		}
	}
	if (output->pipeline_id == WFD_INVALID_PIPELINE_ID) {
		fprintf(stderr, "no pipeline found for port: %d\n", port);
		goto cleanup_pipelines;
	}

	ec->used_pipelines |= (1 << output->pipeline_id);

	wfdGetPortAttribfv(ec->dev, output->port,
			   WFD_PORT_PHYSICAL_SIZE,
			   2, physical_size);

	wlsc_output_init(&output->base, &ec->base, x, y,
			 physical_size[0], physical_size[1], 0);

	output->pipeline = wfdCreatePipeline(ec->dev, output->pipeline_id, NULL);
	if (output->pipeline == WFD_INVALID_HANDLE) {
		fprintf(stderr, "failed to create a pipeline\n");
		goto cleanup_wlsc_output;
	}

	glGenRenderbuffers(2, output->rbo);
	for (i = 0; i < 2; i++) {
		glBindRenderbuffer(GL_RENDERBUFFER, output->rbo[i]);

		output->bo[i] =
			gbm_bo_create(ec->gbm,
				      output->base.current->width,
				      output->base.current->height,
				      GBM_BO_FORMAT_XRGB8888,
				      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
		output->image[i] = ec->base.create_image(ec->base.display,
							 NULL,
							 EGL_NATIVE_PIXMAP_KHR,
							 output->bo[i], NULL);

		printf("output->image[i]: %p\n", output->image[i]);
		ec->base.image_target_renderbuffer_storage(GL_RENDERBUFFER,
							   output->image[i]);
		output->source[i] =
			wfdCreateSourceFromImage(ec->dev, output->pipeline,
						 output->image[i], NULL);

		if (output->source[i] == WFD_INVALID_HANDLE) {
			fprintf(stderr, "failed to create source\n");
			goto cleanup_pipeline;
		}
	}

	output->current = 0;
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  output->rbo[output->current]);

	wfdSetPipelineAttribiv(ec->dev, output->pipeline,
			       WFD_PIPELINE_SOURCE_RECTANGLE,
			       4, geometry.array);
	wfdSetPipelineAttribiv(ec->dev, output->pipeline,
			       WFD_PIPELINE_DESTINATION_RECTANGLE,
			       4, geometry.array);

	wfdBindSourceToPipeline(ec->dev, output->pipeline,
				output->source[output->current ^ 1],
				WFD_TRANSITION_AT_VSYNC, NULL);

	wfdBindPipelineToPort(ec->dev, output->port, output->pipeline);

	wfdDeviceCommit(ec->dev, WFD_COMMIT_ENTIRE_DEVICE, WFD_INVALID_HANDLE);

	output->base.prepare_render = wfd_output_prepare_render;
	output->base.present = wfd_output_present;
	output->base.prepare_scanout_surface =
		wfd_output_prepare_scanout_surface;
	output->base.set_hardware_cursor = wfd_output_set_cursor;
	output->base.destroy = wfd_output_destroy;

	wl_list_insert(ec->base.output_list.prev, &output->base.link);

	return 0;

cleanup_pipeline:
	wfdDestroyPipeline(ec->dev, output->pipeline);
cleanup_wlsc_output:
	wlsc_output_destroy(&output->base);
cleanup_pipelines:
	free(pipelines);
cleanup_port:
	wfdDestroyPort(ec->dev, output->port);
	free(output);

	return -1;
}

static int
create_outputs(struct wfd_compositor *ec, int option_connector)
{
	int x = 0, y = 0;
	WFDint i, num, *ports;
	WFDPort port = WFD_INVALID_HANDLE;

	num = wfdEnumeratePorts(ec->dev, NULL, 0, NULL);
	ports = calloc(num, sizeof *ports);
	if (ports == NULL)
		return -1;

	num = wfdEnumeratePorts(ec->dev, ports, num, NULL);
	if (num < 1)
		return -1;

	for (i = 0; i < num; ++i) {
		port = wfdCreatePort(ec->dev, ports[i], NULL);
		if (port == WFD_INVALID_HANDLE)
			continue;

		if (wfdGetPortAttribi(ec->dev, port, WFD_PORT_ATTACHED) &&
		    (option_connector == 0 || ports[i] == option_connector)) {
			create_output_for_port(ec, port, x, y);

			x += container_of(ec->base.output_list.prev,
					  struct wlsc_output,
					  link)->current->width;
		} else {
			wfdDestroyPort(ec->dev, port);
		}
	}

	free(ports);

	return 0;
}

static int
handle_port_state_change(struct wfd_compositor *ec)
{
	struct wfd_output *output, *next;
	WFDint output_port_id;
	int x = 0, y = 0;
	int x_offset = 0, y_offset = 0;
	WFDPort port;
	WFDint port_id;
	WFDboolean state;

	port_id = wfdGetEventAttribi(ec->dev, ec->event,
				     WFD_EVENT_PORT_ATTACH_PORT_ID);
	state = wfdGetEventAttribi(ec->dev, ec->event,
				   WFD_EVENT_PORT_ATTACH_STATE);

	if (state) {
		struct wlsc_output *last_output =
			container_of(ec->base.output_list.prev,
				     struct wlsc_output, link);

		/* XXX: not yet needed, we die with 0 outputs */
		if (!wl_list_empty(&ec->base.output_list))
			x = last_output->x +
				last_output->current->width;
		else
			x = 0;
		y = 0;

		port = wfdCreatePort(ec->dev, port_id, NULL);
		if (port == WFD_INVALID_HANDLE)
			return -1;

		create_output_for_port(ec, port, x, y);

		return 0;
	}

	wl_list_for_each_safe(output, next, &ec->base.output_list, base.link) {
		output_port_id =
			wfdGetPortAttribi(ec->dev, output->port, WFD_PORT_ID);

		if (!state && output_port_id == port_id) {
			x_offset += output->base.current->width;
			wfd_output_destroy(&output->base);
			continue;
		}

		if (x_offset != 0 || y_offset != 0) {
			wlsc_output_move(&output->base,
					 output->base.x - x_offset,
					 output->base.y - y_offset);
		}
	}

	if (ec->used_pipelines == 0)
		wl_display_terminate(ec->base.wl_display);

	return 0;
}

static int
on_wfd_event(int fd, uint32_t mask, void *data)
{
	struct wfd_compositor *c = data;
	struct wfd_output *output = NULL, *output_iter;
	WFDEventType type;
	const WFDtime timeout = 0;
	WFDint pipeline_id;
	WFDint bind_time;

	type = wfdDeviceEventWait(c->dev, c->event, timeout);

	switch (type) {
	case WFD_EVENT_PIPELINE_BIND_SOURCE_COMPLETE:
		pipeline_id =
			wfdGetEventAttribi(c->dev, c->event,
					   WFD_EVENT_PIPELINE_BIND_PIPELINE_ID);

		bind_time =
			wfdGetEventAttribi(c->dev, c->event,
					   WFD_EVENT_PIPELINE_BIND_TIME_EXT);

		wl_list_for_each(output_iter, &c->base.output_list, base.link) {
			if (output_iter->pipeline_id == pipeline_id)
				output = output_iter;
		}

		if (output == NULL)
			return 1;

		wlsc_output_finish_frame(&output->base,
					 c->start_time + bind_time);
		break;
	case WFD_EVENT_PORT_ATTACH_DETACH:
		handle_port_state_change(c);
		break;
	default:
		return 1;
	}

	return 1;
}

static void
wfd_destroy(struct wlsc_compositor *ec)
{
	struct wfd_compositor *d = (struct wfd_compositor *) ec;

	wlsc_compositor_shutdown(ec);

	udev_unref(d->udev);

	wfdDestroyDevice(d->dev);

	tty_destroy(d->tty);

	free(d);
}

/* FIXME: Just add a stub here for now
 * handle drm{Set,Drop}Master in owfdrm somehow */
static void
vt_func(struct wlsc_compositor *compositor, int event)
{
	return;
}

static const char default_seat[] = "seat0";

static struct wlsc_compositor *
wfd_compositor_create(struct wl_display *display,
		      int connector, const char *seat)
{
	struct wfd_compositor *ec;
	struct wl_event_loop *loop;
	struct timeval tv;

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	memset(ec, 0, sizeof *ec);

	gettimeofday(&tv, NULL);
	ec->start_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	ec->udev = udev_new();
	if (ec->udev == NULL) {
		fprintf(stderr, "failed to initialize udev context\n");
		return NULL;
	}

	ec->dev = wfdCreateDevice(WFD_DEFAULT_DEVICE_ID, NULL);
	if (ec->dev == WFD_INVALID_HANDLE) {
		fprintf(stderr, "failed to create wfd device\n");
		return NULL;
	}

	ec->event = wfdCreateEvent(ec->dev, NULL);
	if (ec->event == WFD_INVALID_HANDLE) {
		fprintf(stderr, "failed to create wfd event\n");
		return NULL;
	}

	ec->base.wl_display = display;
	if (init_egl(ec) < 0) {
		fprintf(stderr, "failed to initialize egl\n");
		return NULL;
	}

	ec->base.destroy = wfd_destroy;
	ec->base.focus = 1;

	glGenFramebuffers(1, &ec->base.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, ec->base.fbo);

	/* Can't init base class until we have a current egl context */
	if (wlsc_compositor_init(&ec->base, display) < 0)
		return NULL;

	if (create_outputs(ec, connector) < 0) {
		fprintf(stderr, "failed to create outputs\n");
		return NULL;
	}

	evdev_input_add_devices(&ec->base, ec->udev, seat);

	loop = wl_display_get_event_loop(ec->base.wl_display);
	ec->wfd_source =
		wl_event_loop_add_fd(loop,
				     wfdDeviceEventGetFD(ec->dev, ec->event),
				     WL_EVENT_READABLE, on_wfd_event, ec);
	ec->tty = tty_create(&ec->base, vt_func);

	return &ec->base;
}

struct wlsc_compositor *
backend_init(struct wl_display *display, char *options);

WL_EXPORT struct wlsc_compositor *
backend_init(struct wl_display *display, char *options)
{
	int connector = 0, i;
	const char *seat;
	char *p, *value;

	static char * const tokens[] = { "connector", "seat", NULL };
	
	p = options;
	seat = default_seat;
	while (i = getsubopt(&p, tokens, &value), i != -1) {
		switch (i) {
		case 0:
			connector = strtol(value, NULL, 0);
			break;
		case 1:
			seat = value;
			break;
		}
	}

	return wfd_compositor_create(display, connector, seat);
}

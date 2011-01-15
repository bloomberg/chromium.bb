/*
 * Copyright © 2011 Benjamin Franzke
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include <fcntl.h>

#include <wayland-client.h>
#include <xf86drm.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct display {
	struct wl_display *display;
	struct {
		struct wl_compositor *compositor;
		struct wl_drm *drm;
	} interface;
	struct {
		EGLDisplay dpy;
		EGLContext ctx;
	} egl;
	struct {
		int fd;
		const char *device_name;
		bool authenticated;
	} drm;
	uint32_t mask;
};

struct window {
	struct display *display;
	struct {
		int width, height;
	} geometry;
	struct {
		GLuint fbo;
		GLuint color_rbo;

		GLuint program;
		GLuint rotation_uniform;

		GLuint pos;
		GLuint col;
	} gl;
	struct {
		struct wl_buffer *buffer;
		struct wl_surface *surface;
		EGLImageKHR image;
	} drm_surface;
};

static const char *vert_shader_text =
	"uniform mat4 rotation;\n"
	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_Position = rotation * pos;\n"
	"  v_color = color;\n"
	"}\n";

static const char *frag_shader_text =
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"
	"}\n";

static void
init_egl(struct display *display)
{
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	EGLint major, minor;
	EGLBoolean ret;

	display->egl.dpy = eglGetDRMDisplayMESA(display->drm.fd);
	assert(display->egl.dpy);

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	display->egl.ctx = eglCreateContext(display->egl.dpy, NULL,
					    EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);
	ret = eglMakeCurrent(display->egl.dpy, NULL, NULL, display->egl.ctx);
	assert(ret == EGL_TRUE);
}

static GLuint
create_shader(struct window *window, const char *source, GLenum shader_type)
{
	GLuint shader;
	GLint status;

	shader = glCreateShader(shader_type);
	assert(shader != 0);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Error: compiling %s: %*s\n",
			shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
			len, log);
		exit(1);
	}

	return shader;
}

static void
init_gl(struct window *window)
{
	GLfloat ar;
	GLuint frag, vert;
	GLint status;

	glGenFramebuffers(1, &window->gl.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, window->gl.fbo);

	glGenRenderbuffers(1, &window->gl.color_rbo);

	glViewport(0, 0, window->geometry.width, window->geometry.height);
	ar = (GLfloat)window->geometry.width / (GLfloat)window->geometry.height;

	frag = create_shader(window, frag_shader_text, GL_FRAGMENT_SHADER);
	vert = create_shader(window, vert_shader_text, GL_VERTEX_SHADER);

	window->gl.program = glCreateProgram();
	glAttachShader(window->gl.program, frag);
	glAttachShader(window->gl.program, vert);
	glLinkProgram(window->gl.program);

	glGetProgramiv(window->gl.program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(window->gl.program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}

	glUseProgram(window->gl.program);
	
	window->gl.pos = 0;
	window->gl.pos = 1;

	glBindAttribLocation(window->gl.program, window->gl.pos, "pos");
	glBindAttribLocation(window->gl.program, window->gl.col, "color");
	glLinkProgram(window->gl.program);

	window->gl.rotation_uniform =
		glGetUniformLocation(window->gl.program, "rotation");
}

static void
create_surface(struct window *window)
{
	struct display *display = window->display;
	struct wl_visual *visual;
	EGLint name, stride;
	EGLint image_attribs[] = {
		EGL_WIDTH,			0,
		EGL_HEIGHT,			0,
		EGL_DRM_BUFFER_FORMAT_MESA,	EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA,	EGL_DRM_BUFFER_USE_SCANOUT_MESA,
		EGL_NONE
	};

	window->drm_surface.surface =
		wl_compositor_create_surface(display->interface.compositor);

	image_attribs[1] = window->geometry.width;
	image_attribs[3] = window->geometry.height;

	window->drm_surface.image = eglCreateDRMImageMESA(display->egl.dpy,
							  image_attribs);
	eglExportDRMImageMESA(display->egl.dpy, window->drm_surface.image,
			      &name, NULL, &stride);
	visual = wl_display_get_premultiplied_argb_visual(display->display);

	window->drm_surface.buffer =
		wl_drm_create_buffer(display->interface.drm, name,
				     window->geometry.width,
				     window->geometry.height,
				     stride, visual);

	wl_surface_attach(window->drm_surface.surface,
			  window->drm_surface.buffer, 0, 0);
	wl_surface_map_toplevel(window->drm_surface.surface);

	glBindRenderbuffer(GL_RENDERBUFFER, window->gl.color_rbo);
	glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
					       window->drm_surface.image);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				  GL_COLOR_ATTACHMENT0,
				  GL_RENDERBUFFER,
				  window->gl.color_rbo);

	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
			GL_FRAMEBUFFER_COMPLETE);
}

static void
redraw(void *data, uint32_t time)
{
	struct window *window = data;
	static const GLfloat verts[3][2] = {
		{ -0.5, -0.5 },
		{  0.5, -0.5 },
		{  0,    0.5 }
	};
	static const GLfloat colors[3][3] = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 }
	};
	GLfloat angle;
	GLfloat rotation[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};
	static const int32_t speed_div = 5;
	static uint32_t start_time = 0;

	if (start_time == 0)
		start_time = time;

	angle = ((time-start_time) / speed_div) % 360 * M_PI / 180.0;
	rotation[0][0] =  cos(angle);
	rotation[0][2] =  sin(angle);
	rotation[2][0] = -sin(angle);
	rotation[2][2] =  cos(angle);

	glUniformMatrix4fv(window->gl.rotation_uniform, 1, GL_FALSE,
			   (GLfloat *) rotation);

	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(window->gl.pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(window->gl.col, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(window->gl.pos);
	glEnableVertexAttribArray(window->gl.col);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(window->gl.pos);
	glDisableVertexAttribArray(window->gl.col);

	glFlush();

	wl_surface_damage(window->drm_surface.surface, 0, 0,
			  window->geometry.width, window->geometry.height);

	wl_display_frame_callback(window->display->display, redraw, window);
}

static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
	struct display *d = data;

	d->drm.device_name = strdup(device);
}

static void
drm_handle_authenticated(void *data, struct wl_drm *drm)
{
	struct display *d = data;

	d->drm.authenticated = true;
}

static const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_authenticated
};

static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	struct display *d = data;

	if (strcmp(interface, "compositor") == 0) {
		d->interface.compositor = wl_compositor_create(display, id);
	} else if (strcmp(interface, "drm") == 0) {
		d->interface.drm = wl_drm_create(display, id);
		wl_drm_add_listener(d->interface.drm, &drm_listener, d);
	}
}

static int
event_mask_update(uint32_t mask, void *data)
{
	struct display *d = data;

	d->mask = mask;

	return 0;
}

int
main(int argc, char **argv)
{
	struct display display = { 0 };
	struct window  window  = { 0 };
	drm_magic_t magic;
	int ret;

	memset(&display, 0, sizeof display);
	memset(&window,  0, sizeof window);

	window.display = &display;
	window.geometry.width  = 250;
	window.geometry.height = 250;

	display.display = wl_display_connect(NULL);
	assert(display.display);

	wl_display_add_global_listener(display.display,
				    display_handle_global, &display);
	/* process connection events */
	wl_display_iterate(display.display, WL_DISPLAY_READABLE);

	display.drm.fd = open(display.drm.device_name, O_RDWR);
	assert(display.drm.fd >= 0);

	ret = drmGetMagic(display.drm.fd, &magic);
	assert(ret == 0);
	wl_drm_authenticate(display.interface.drm, magic);
	wl_display_iterate(display.display, WL_DISPLAY_WRITABLE);
	while (!display.drm.authenticated)
		wl_display_iterate(display.display, WL_DISPLAY_READABLE);

	init_egl(&display);
	init_gl(&window);
	create_surface(&window);

	wl_display_frame_callback(display.display, redraw, &window);

	wl_display_get_fd(display.display, event_mask_update, &display);
	while (true)
		wl_display_iterate(display.display, display.mask);
	
	return 0;
}

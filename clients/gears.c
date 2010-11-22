/*
 * Copyright © 2008 Kristian Høgsberg
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>
#include <glib.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "wayland-util.h"
#include "wayland-client.h"
#include "wayland-glib.h"

#include "window.h"

struct gears {
	struct window *window;

	struct display *d;
	struct rectangle rectangle;

	EGLDisplay display;
	EGLContext context;
	int drm_fd;
	GLfloat angle;
	cairo_surface_t *cairo_surface;

	GLint gear_list[3];
	GLuint fbo, color_rbo[2], depth_rbo;
	cairo_surface_t *surface[2];
	int current;
};

struct gear_template {
	GLfloat material[4];
	GLfloat inner_radius;
	GLfloat outer_radius;
	GLfloat width;
	GLint teeth;
	GLfloat tooth_depth;
};

const static struct gear_template gear_templates[] = {
	{ { 0.8, 0.1, 0.0, 1.0 }, 1.0, 4.0, 1.0, 20, 0.7 },
	{ { 0.0, 0.8, 0.2, 1.0 }, 0.5, 2.0, 2.0, 10, 0.7 },
	{ { 0.2, 0.2, 1.0, 1.0 }, 1.3, 2.0, 0.5, 10, 0.7 }, 
};

static GLfloat light_pos[4] = {5.0, 5.0, 10.0, 0.0};

static void die(const char *msg)
{
	fprintf(stderr, "%s", msg);
	exit(EXIT_FAILURE);
}

static void
make_gear(const struct gear_template *t)
{
	GLint i;
	GLfloat r0, r1, r2;
	GLfloat angle, da;
	GLfloat u, v, len;

	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, t->material);

	r0 = t->inner_radius;
	r1 = t->outer_radius - t->tooth_depth / 2.0;
	r2 = t->outer_radius + t->tooth_depth / 2.0;

	da = 2.0 * M_PI / t->teeth / 4.0;

	glShadeModel(GL_FLAT);

	glNormal3f(0.0, 0.0, 1.0);

	/* draw front face */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= t->teeth; i++) {
		angle = i * 2.0 * M_PI / t->teeth;
		glVertex3f(r0 * cos(angle), r0 * sin(angle), t->width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), t->width * 0.5);
		if (i < t->teeth) {
			glVertex3f(r0 * cos(angle), r0 * sin(angle), t->width * 0.5);
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), t->width * 0.5);
		}
	}
	glEnd();

	/* draw front sides of teeth */
	glBegin(GL_QUADS);
	da = 2.0 * M_PI / t->teeth / 4.0;
	for (i = 0; i < t->teeth; i++) {
		angle = i * 2.0 * M_PI / t->teeth;

		glVertex3f(r1 * cos(angle), r1 * sin(angle), t->width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), t->width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), t->width * 0.5);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), t->width * 0.5);
	}
	glEnd();

	glNormal3f(0.0, 0.0, -1.0);

	/* draw back face */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= t->teeth; i++) {
		angle = i * 2.0 * M_PI / t->teeth;
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -t->width * 0.5);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), -t->width * 0.5);
		if (i < t->teeth) {
			glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -t->width * 0.5);
			glVertex3f(r0 * cos(angle), r0 * sin(angle), -t->width * 0.5);
		}
	}
	glEnd();

	/* draw back sides of teeth */
	glBegin(GL_QUADS);
	da = 2.0 * M_PI / t->teeth / 4.0;
	for (i = 0; i < t->teeth; i++) {
		angle = i * 2.0 * M_PI / t->teeth;

		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -t->width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -t->width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -t->width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -t->width * 0.5);
	}
	glEnd();

	/* draw outward faces of teeth */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < t->teeth; i++) {
		angle = i * 2.0 * M_PI / t->teeth;

		glVertex3f(r1 * cos(angle), r1 * sin(angle), t->width * 0.5);
		glVertex3f(r1 * cos(angle), r1 * sin(angle), -t->width * 0.5);
		u = r2 * cos(angle + da) - r1 * cos(angle);
		v = r2 * sin(angle + da) - r1 * sin(angle);
		len = sqrt(u * u + v * v);
		u /= len;
		v /= len;
		glNormal3f(v, -u, 0.0);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), t->width * 0.5);
		glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -t->width * 0.5);
		glNormal3f(cos(angle), sin(angle), 0.0);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), t->width * 0.5);
		glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), -t->width * 0.5);
		u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
		v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
		glNormal3f(v, -u, 0.0);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), t->width * 0.5);
		glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), -t->width * 0.5);
		glNormal3f(cos(angle), sin(angle), 0.0);
	}

	glVertex3f(r1 * cos(0), r1 * sin(0), t->width * 0.5);
	glVertex3f(r1 * cos(0), r1 * sin(0), -t->width * 0.5);

	glEnd();

	glShadeModel(GL_SMOOTH);

	/* draw inside radius cylinder */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= t->teeth; i++) {
		angle = i * 2.0 * M_PI / t->teeth;
		glNormal3f(-cos(angle), -sin(angle), 0.0);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), -t->width * 0.5);
		glVertex3f(r0 * cos(angle), r0 * sin(angle), t->width * 0.5);
	}
	glEnd();
}

static void
allocate_buffer(struct gears *gears)
{
	EGLImageKHR image;

	/* Constrain child size to be square and at least 300x300 */
	window_get_child_rectangle(gears->window, &gears->rectangle);
	if (gears->rectangle.width > gears->rectangle.height)
		gears->rectangle.height = gears->rectangle.width;
	else
		gears->rectangle.width = gears->rectangle.height;
	if (gears->rectangle.width < 300) {
		gears->rectangle.width = 300;
		gears->rectangle.height = 300;
	}

	window_set_child_size(gears->window, &gears->rectangle);
	window_draw(gears->window);

	gears->surface[gears->current] = window_get_surface(gears->window);

	image = display_get_image_for_drm_surface(gears->display,
						  gears->surface[gears->current]);

	if (!eglMakeCurrent(gears->display, NULL, NULL, gears->context))
		die("faile to make context current\n");

	glBindRenderbuffer(GL_RENDERBUFFER_EXT,
			   gears->color_rbo[gears->current]);
	glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);

	glBindRenderbuffer(GL_RENDERBUFFER_EXT, gears->depth_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER_EXT,
			      GL_DEPTH_COMPONENT,
			      gears->rectangle.width + 20 + 32,
			      gears->rectangle.height + 60 + 32);
}

static void
draw_gears(struct gears *gears)
{
	GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;

	if (gears->surface[gears->current] == NULL)
		allocate_buffer(gears);

	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER_EXT,
				  GL_COLOR_ATTACHMENT0_EXT,
				  GL_RENDERBUFFER_EXT,
				  gears->color_rbo[gears->current]);

	glViewport(gears->rectangle.x, gears->rectangle.y,
		   gears->rectangle.width, gears->rectangle.height);
	glScissor(gears->rectangle.x, gears->rectangle.y,
		   gears->rectangle.width, gears->rectangle.height);

	glEnable(GL_SCISSOR_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -50);

	glRotatef(view_rotx, 1.0, 0.0, 0.0);
	glRotatef(view_roty, 0.0, 1.0, 0.0);
	glRotatef(view_rotz, 0.0, 0.0, 1.0);

	glPushMatrix();
	glTranslatef(-3.0, -2.0, 0.0);
	glRotatef(gears->angle, 0.0, 0.0, 1.0);
	glCallList(gears->gear_list[0]);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(3.1, -2.0, 0.0);
	glRotatef(-2.0 * gears->angle - 9.0, 0.0, 0.0, 1.0);
	glCallList(gears->gear_list[1]);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(-3.1, 4.2, 0.0);
	glRotatef(-2.0 * gears->angle - 25.0, 0.0, 0.0, 1.0);
	glCallList(gears->gear_list[2]);
	glPopMatrix();

	glPopMatrix();

	glFlush();

	window_set_surface(gears->window, gears->surface[gears->current]);
	window_flush(gears->window);
}

static void
resize_handler(struct window *window, void *data)
{
	struct gears *gears = data;

	cairo_surface_destroy(gears->surface[0]);
	gears->surface[0] = NULL;
	cairo_surface_destroy(gears->surface[1]);
	gears->surface[1] = NULL;
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct gears *gears = data;

	resize_handler(window, gears);
}

static void
redraw_handler(struct window *window, void *data)
{
	struct gears *gears = data;

	draw_gears(gears);
}

static void
frame_callback(void *data, uint32_t time)
{
	struct gears *gears = data;

	gears->current = 1 - gears->current;

	gears->angle = (GLfloat) (time % 8192) * 360 / 8192.0;

	window_schedule_redraw(gears->window);
	wl_display_frame_callback(display_get_display(gears->d),
				  frame_callback, gears);
}

static struct gears *
gears_create(struct display *display)
{
	const int x = 200, y = 200, width = 450, height = 500;
	struct gears *gears;
	int i;

	gears = malloc(sizeof *gears);
	memset(gears, 0, sizeof *gears);
	gears->d = display;
	gears->window = window_create(display, "Wayland Gears",
				      x, y, width, height);

	gears->display = display_get_egl_display(gears->d);
	if (gears->display == NULL)
		die("failed to create egl display\n");

	eglBindAPI(EGL_OPENGL_API);

	gears->context = eglCreateContext(gears->display,
					  NULL, EGL_NO_CONTEXT, NULL);
	if (gears->context == NULL)
		die("failed to create context\n");

	if (!eglMakeCurrent(gears->display, NULL, NULL, gears->context))
		die("faile to make context current\n");

	glGenFramebuffers(1, &gears->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, gears->fbo);

	glGenRenderbuffers(2, gears->color_rbo);
	glGenRenderbuffers(1, &gears->depth_rbo);
	glBindRenderbuffer(GL_RENDERBUFFER_EXT, gears->depth_rbo);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER_EXT,
				  GL_DEPTH_ATTACHMENT_EXT,
				  GL_RENDERBUFFER_EXT,
				  gears->depth_rbo);
	for (i = 0; i < 3; i++) {
		gears->gear_list[i] = glGenLists(1);
		glNewList(gears->gear_list[i], GL_COMPILE);
		make_gear(&gear_templates[i]);
		glEndList();
	}

	glEnable(GL_NORMALIZE);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 200.0);
	glMatrixMode(GL_MODELVIEW);

	glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0, 0, 0, 0.92);

	window_set_user_data(gears->window, gears);
	window_set_resize_handler(gears->window, resize_handler);
	window_set_keyboard_focus_handler(gears->window, keyboard_focus_handler);
	window_set_redraw_handler(gears->window, redraw_handler);

	draw_gears(gears);
	wl_display_frame_callback(display_get_display(gears->d),
				  frame_callback, gears);

	return gears;
}

int main(int argc, char *argv[])
{
	struct display *d;
	struct gears *gears;

	d = display_create(&argc, &argv, NULL);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}
	gears = gears_create(d);
	display_run(d);

	return 0;
}

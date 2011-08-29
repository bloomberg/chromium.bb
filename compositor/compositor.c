/*
 * Copyright © 2008 Kristian Høgsberg
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <linux/input.h>
#include <dlfcn.h>
#include <getopt.h>
#include <signal.h>

#include "wayland-server.h"
#include "compositor.h"

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char *option_socket_name = NULL;
static const char *option_background = "background.png";
static int option_idle_time = 300;

static struct wl_list child_process_list;

static int
sigchld_handler(int signal_number, void *data)
{
	struct wlsc_process *p;
	int status;
	pid_t pid;

	pid = wait(&status);
	wl_list_for_each(p, &child_process_list, link) {
		if (p->pid == pid)
			break;
	}

	if (&p->link == &child_process_list) {
		fprintf(stderr, "unknown child process exited\n");
		return 1;
	}

	wl_list_remove(&p->link);
	p->cleanup(p, status);

	return 1;
}

WL_EXPORT void
wlsc_watch_process(struct wlsc_process *process)
{
	wl_list_insert(&child_process_list, &process->link);
}

WL_EXPORT void
wlsc_matrix_init(struct wlsc_matrix *matrix)
{
	static const struct wlsc_matrix identity = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }
	};

	memcpy(matrix, &identity, sizeof identity);
}

static void
wlsc_matrix_multiply(struct wlsc_matrix *m, const struct wlsc_matrix *n)
{
	struct wlsc_matrix tmp;
	const GLfloat *row, *column;
	div_t d;
	int i, j;

	for (i = 0; i < 16; i++) {
		tmp.d[i] = 0;
		d = div(i, 4);
		row = m->d + d.quot * 4;
		column = n->d + d.rem;
		for (j = 0; j < 4; j++)
			tmp.d[i] += row[j] * column[j * 4];
	}
	memcpy(m, &tmp, sizeof tmp);
}

WL_EXPORT void
wlsc_matrix_translate(struct wlsc_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct wlsc_matrix translate = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 }
	};

	wlsc_matrix_multiply(matrix, &translate);
}

WL_EXPORT void
wlsc_matrix_scale(struct wlsc_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct wlsc_matrix scale = {
		{ x, 0, 0, 0,  0, y, 0, 0,  0, 0, z, 0,  0, 0, 0, 1 }
	};

	wlsc_matrix_multiply(matrix, &scale);
}

static void
wlsc_matrix_transform(struct wlsc_matrix *matrix, struct wlsc_vector *v)
{
	int i, j;
	struct wlsc_vector t;

	for (i = 0; i < 4; i++) {
		t.f[i] = 0;
		for (j = 0; j < 4; j++)
			t.f[i] += v->f[j] * matrix->d[i + j * 4];
	}

	*v = t;
}

WL_EXPORT void
wlsc_spring_init(struct wlsc_spring *spring,
		 double k, double current, double target)
{
	spring->k = k;
	spring->friction = 100.0;
	spring->current = current;
	spring->previous = current;
	spring->target = target;
}

WL_EXPORT void
wlsc_spring_update(struct wlsc_spring *spring, uint32_t msec)
{
	double force, v, current, step;

	step = (msec - spring->timestamp) / 300.0;
	spring->timestamp = msec;

	current = spring->current;
	v = current - spring->previous;
	force = spring->k * (spring->target - current) / 10.0 +
		(spring->previous - current) - v * spring->friction;

	spring->current =
		current + (current - spring->previous) + force * step * step;
	spring->previous = current;

#if 0
	if (spring->current >= 1.0) {
#ifdef TWEENER_BOUNCE
		spring->current = 2.0 - spring->current;
		spring->previous = 2.0 - spring->previous;
#else
		spring->current = 1.0;
		spring->previous = 1.0;
#endif
	}

	if (spring->current <= 0.0) {
		spring->current = 0.0;
		spring->previous = 0.0;
	}
#endif
}

WL_EXPORT int
wlsc_spring_done(struct wlsc_spring *spring)
{
	return fabs(spring->previous - spring->target) < 0.0002 &&
		fabs(spring->current - spring->target) < 0.0002;
}

static void
surface_handle_buffer_destroy(struct wl_listener *listener,
			      struct wl_resource *resource, uint32_t time)
{
	struct wlsc_surface *es = container_of(listener, struct wlsc_surface,
					       buffer_destroy_listener);
	struct wl_buffer *buffer = (struct wl_buffer *) resource;

	if (es->buffer == buffer)
		es->buffer = NULL;
}

static void
output_handle_scanout_buffer_destroy(struct wl_listener *listener,
				     struct wl_resource *resource,
				     uint32_t time)
{
	struct wlsc_output *output =
		container_of(listener, struct wlsc_output,
			     scanout_buffer_destroy_listener);
	struct wl_buffer *buffer = (struct wl_buffer *) resource;

	if (output->scanout_buffer == buffer)
		output->scanout_buffer = NULL;
}

WL_EXPORT struct wlsc_surface *
wlsc_surface_create(struct wlsc_compositor *compositor,
		    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *surface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	wl_list_init(&surface->link);
	wl_list_init(&surface->buffer_link);
	surface->map_type = WLSC_SURFACE_MAP_UNMAPPED;

	glGenTextures(1, &surface->texture);
	glBindTexture(GL_TEXTURE_2D, surface->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	surface->surface.client = NULL;

	surface->compositor = compositor;
	surface->visual = NULL;
	surface->image = EGL_NO_IMAGE_KHR;
	surface->saved_texture = 0;
	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;

	surface->buffer = NULL;

	pixman_region32_init(&surface->damage);
	pixman_region32_init(&surface->opaque);

	surface->buffer_destroy_listener.func = surface_handle_buffer_destroy;
	wl_list_init(&surface->buffer_destroy_listener.link);

	surface->transform = NULL;

	return surface;
}

WL_EXPORT void
wlsc_surface_damage_rectangle(struct wlsc_surface *surface,
			      int32_t x, int32_t y,
			      int32_t width, int32_t height)
{
	struct wlsc_compositor *compositor = surface->compositor;

	pixman_region32_union_rect(&surface->damage,
				   &surface->damage,
				   surface->x + x, surface->y + y,
				   width, height);
	wlsc_compositor_schedule_repaint(compositor);
}

WL_EXPORT void
wlsc_surface_damage(struct wlsc_surface *surface)
{
	wlsc_surface_damage_rectangle(surface, 0, 0,
				      surface->width, surface->height);
}

WL_EXPORT void
wlsc_surface_damage_below(struct wlsc_surface *surface)
{
	struct wlsc_surface *below;

	if (surface->link.next == &surface->compositor->surface_list)
		return;

	below = container_of(surface->link.next, struct wlsc_surface, link);

	pixman_region32_union_rect(&below->damage,
				   &below->damage,
				   surface->x, surface->y,
				   surface->width, surface->height);
	wlsc_compositor_schedule_repaint(surface->compositor);
}

WL_EXPORT void
wlsc_surface_configure(struct wlsc_surface *surface,
		       int x, int y, int width, int height)
{
	struct wlsc_compositor *compositor = surface->compositor;

	wlsc_surface_damage_below(surface);

	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;

	wlsc_surface_assign_output(surface);
	wlsc_surface_damage(surface);

	pixman_region32_fini(&surface->opaque);
	if (surface->visual == &compositor->compositor.rgb_visual)
		pixman_region32_init_rect(&surface->opaque,
					  surface->x, surface->y,
					  surface->width, surface->height);
	else
		pixman_region32_init(&surface->opaque);
}

WL_EXPORT uint32_t
wlsc_compositor_get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
destroy_surface(struct wl_resource *resource, struct wl_client *client)
{
	struct wlsc_surface *surface =
		container_of(resource, struct wlsc_surface, surface.resource);
	struct wlsc_compositor *compositor = surface->compositor;

	wlsc_surface_damage_below(surface);

	wl_list_remove(&surface->link);
	if (surface->saved_texture == 0)
		glDeleteTextures(1, &surface->texture);
	else
		glDeleteTextures(1, &surface->saved_texture);

	if (surface->buffer)
		wl_list_remove(&surface->buffer_destroy_listener.link);

	if (surface->image != EGL_NO_IMAGE_KHR)
		compositor->destroy_image(compositor->display,
					  surface->image);

	wl_list_remove(&surface->buffer_link);

	free(surface);
}

static void
wlsc_buffer_attach(struct wl_buffer *buffer, struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;
	struct wl_list *surfaces_attached_to;

	if (es->saved_texture != 0)
		es->texture = es->saved_texture;

	glBindTexture(GL_TEXTURE_2D, es->texture);

	if (wl_buffer_is_shm(buffer)) {
		/* Unbind any EGLImage texture that may be bound, so we don't
		 * overwrite it.*/
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     0, 0, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		es->pitch = wl_shm_buffer_get_stride(buffer) / 4;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     es->pitch, buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(buffer));
		es->visual = buffer->visual;

		surfaces_attached_to = buffer->user_data;

		wl_list_remove(&es->buffer_link);
		wl_list_insert(surfaces_attached_to, &es->buffer_link);
	} else {
		if (es->image != EGL_NO_IMAGE_KHR)
			ec->destroy_image(ec->display, es->image);
		es->image = ec->create_image(ec->display, NULL,
					     EGL_WAYLAND_BUFFER_WL,
					     buffer, NULL);
		
		ec->image_target_texture_2d(GL_TEXTURE_2D, es->image);
		es->visual = buffer->visual;
		es->pitch = es->width;
	}
}

static void
wlsc_sprite_attach(struct wlsc_sprite *sprite, struct wl_surface *surface)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;

	es->pitch = es->width;
	es->image = sprite->image;
	if (sprite->image != EGL_NO_IMAGE_KHR) {
		glBindTexture(GL_TEXTURE_2D, es->texture);
		ec->image_target_texture_2d(GL_TEXTURE_2D, es->image);
	} else {
		if (es->saved_texture == 0)
			es->saved_texture = es->texture;
		es->texture = sprite->texture;
	}

	es->visual = sprite->visual;

	if (es->buffer)
		es->buffer = NULL;
}

enum sprite_usage {
	SPRITE_USE_CURSOR = (1 << 0),
};

static struct wlsc_sprite *
create_sprite_from_png(struct wlsc_compositor *ec,
		       const char *filename, uint32_t usage)
{
	uint32_t *pixels;
	struct wlsc_sprite *sprite;
	int32_t width, height;
	uint32_t stride;

	pixels = wlsc_load_image(filename, &width, &height, &stride);
	if (pixels == NULL)
		return NULL;

	sprite = malloc(sizeof *sprite);
	if (sprite == NULL) {
		free(pixels);
		return NULL;
	}

	sprite->visual = &ec->compositor.premultiplied_argb_visual;
	sprite->width = width;
	sprite->height = height;
	sprite->image = EGL_NO_IMAGE_KHR;

	if (usage & SPRITE_USE_CURSOR && ec->create_cursor_image != NULL)
		sprite->image = ec->create_cursor_image(ec, width, height);

	glGenTextures(1, &sprite->texture);
	glBindTexture(GL_TEXTURE_2D, sprite->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (sprite->image != EGL_NO_IMAGE_KHR) {
		ec->image_target_texture_2d(GL_TEXTURE_2D, sprite->image);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
				GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, width, height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
	}

	free(pixels);

	return sprite;
}

static const struct {
	const char *filename;
	int hotspot_x, hotspot_y;
} pointer_images[] = {
	{ DATADIR "/wayland/bottom_left_corner.png",	 6, 30 },
	{ DATADIR "/wayland/bottom_right_corner.png",	28, 28 },
	{ DATADIR "/wayland/bottom_side.png",		16, 20 },
	{ DATADIR "/wayland/grabbing.png",		20, 17 },
	{ DATADIR "/wayland/left_ptr.png",		10,  5 },
	{ DATADIR "/wayland/left_side.png",		10, 20 },
	{ DATADIR "/wayland/right_side.png",		30, 19 },
	{ DATADIR "/wayland/top_left_corner.png",	 8,  8 },
	{ DATADIR "/wayland/top_right_corner.png",	26,  8 },
	{ DATADIR "/wayland/top_side.png",		18,  8 },
	{ DATADIR "/wayland/xterm.png",			15, 15 }
};

static void
create_pointer_images(struct wlsc_compositor *ec)
{
	int i, count;

	count = ARRAY_LENGTH(pointer_images);
	ec->pointer_sprites = malloc(count * sizeof *ec->pointer_sprites);
	for (i = 0; i < count; i++) {
		ec->pointer_sprites[i] =
			create_sprite_from_png(ec,
					       pointer_images[i].filename,
					       SPRITE_USE_CURSOR);
	}
}

static struct wlsc_surface *
background_create(struct wlsc_output *output, const char *filename)
{
	struct wlsc_surface *background;
	struct wlsc_sprite *sprite;

	background = wlsc_surface_create(output->compositor,
					 output->x, output->y,
					 output->current->width,
					 output->current->height);
	if (background == NULL)
		return NULL;

	sprite = create_sprite_from_png(output->compositor, filename, 0);
	if (sprite == NULL) {
		free(background);
		return NULL;
	}

	wlsc_sprite_attach(sprite, &background->surface);

	return background;
}

static int
texture_region(struct wlsc_surface *es, pixman_region32_t *region)
{
	struct wlsc_compositor *ec = es->compositor;
	GLfloat *v, inv_width, inv_height;
	pixman_box32_t *rectangles;
	unsigned int *p;
	int i, n;

	rectangles = pixman_region32_rectangles(region, &n);
	v = wl_array_add(&ec->vertices, n * 16 * sizeof *v);
	p = wl_array_add(&ec->indices, n * 6 * sizeof *p);
	inv_width = 1.0 / es->pitch;
	inv_height = 1.0 / es->height;

	for (i = 0; i < n; i++, v += 16, p += 6) {
		v[ 0] = rectangles[i].x1;
		v[ 1] = rectangles[i].y1;
		v[ 2] = (GLfloat) (rectangles[i].x1 - es->x) * inv_width;
		v[ 3] = (GLfloat) (rectangles[i].y1 - es->y) * inv_height;

		v[ 4] = rectangles[i].x1;
		v[ 5] = rectangles[i].y2;
		v[ 6] = v[ 2];
		v[ 7] = (GLfloat) (rectangles[i].y2 - es->y) * inv_height;

		v[ 8] = rectangles[i].x2;
		v[ 9] = rectangles[i].y1;
		v[10] = (GLfloat) (rectangles[i].x2 - es->x) * inv_width;
		v[11] = v[ 3];

		v[12] = rectangles[i].x2;
		v[13] = rectangles[i].y2;
		v[14] = v[10];
		v[15] = v[ 7];

		p[0] = i * 4 + 0;
		p[1] = i * 4 + 1;
		p[2] = i * 4 + 2;
		p[3] = i * 4 + 2;
		p[4] = i * 4 + 1;
		p[5] = i * 4 + 3;
	}

	return n;
}

static void
transform_vertex(struct wlsc_surface *surface,
		 GLfloat x, GLfloat y, GLfloat u, GLfloat v, GLfloat *r)
{
	struct wlsc_vector t;

	t.f[0] = x;
	t.f[1] = y;
	t.f[2] = 0.0;
	t.f[3] = 1.0;

	wlsc_matrix_transform(&surface->transform->matrix, &t);

	r[ 0] = t.f[0];
	r[ 1] = t.f[1];
	r[ 2] = u;
	r[ 3] = v;
}

static int
texture_transformed_surface(struct wlsc_surface *es)
{
	struct wlsc_compositor *ec = es->compositor;
	GLfloat *v;
	unsigned int *p;

	v = wl_array_add(&ec->vertices, 16 * sizeof *v);
	p = wl_array_add(&ec->indices, 6 * sizeof *p);

	transform_vertex(es, es->x, es->y, 0.0, 0.0, &v[0]);
	transform_vertex(es, es->x, es->y + es->height, 0.0, 1.0, &v[4]);
	transform_vertex(es, es->x + es->width, es->y, 1.0, 0.0, &v[8]);
	transform_vertex(es, es->x + es->width, es->y + es->height,
			 1.0, 1.0, &v[12]);

	p[0] = 0;
	p[1] = 1;
	p[2] = 2;
	p[3] = 2;
	p[4] = 1;
	p[5] = 3;

	return 1;
}

static void
wlsc_surface_draw(struct wlsc_surface *es,
		  struct wlsc_output *output, pixman_region32_t *clip)
{
	struct wlsc_compositor *ec = es->compositor;
	GLfloat *v;
	pixman_region32_t repaint;
	GLint filter;
	int n;

	pixman_region32_init_rect(&repaint,
				  es->x, es->y, es->width, es->height);
	pixman_region32_intersect(&repaint, &repaint, clip);
	if (!pixman_region32_not_empty(&repaint))
		return;

	if (es->visual == &ec->compositor.argb_visual) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else if (es->visual == &ec->compositor.premultiplied_argb_visual) {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}

	if (es->transform == NULL) {
		filter = GL_NEAREST;
		n = texture_region(es, &repaint);
	} else {
		filter = GL_LINEAR;
		n = texture_transformed_surface(es);
	}

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	v = ec->vertices.data;
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[0]);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[2]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawElements(GL_TRIANGLES, n * 6, GL_UNSIGNED_INT, ec->indices.data);

	ec->vertices.size = 0;
	ec->indices.size = 0;
	pixman_region32_fini(&repaint);
}

static void
wlsc_surface_raise(struct wlsc_surface *surface)
{
	struct wlsc_compositor *compositor = surface->compositor;

	wl_list_remove(&surface->link);
	wl_list_insert(&compositor->surface_list, &surface->link);
	wlsc_surface_damage(surface);
}

WL_EXPORT void
wlsc_compositor_damage_all(struct wlsc_compositor *compositor)
{
	struct wlsc_output *output;

	wl_list_for_each(output, &compositor->output_list, link)
		wlsc_output_damage(output);
}

static inline void
wlsc_buffer_post_release(struct wl_buffer *buffer)
{
	if (buffer == NULL || --buffer->busy_count > 0)
		return;

	assert(buffer->client != NULL);
	wl_client_post_event(buffer->client,
			     &buffer->resource.object,
			     WL_BUFFER_RELEASE);
}

WL_EXPORT void
wlsc_output_damage(struct wlsc_output *output)
{
	struct wlsc_compositor *compositor = output->compositor;

	pixman_region32_union(&compositor->damage,
			      &compositor->damage, &output->region);
	wlsc_compositor_schedule_repaint(compositor);
}

static void
fade_frame(struct wlsc_animation *animation,
	   struct wlsc_output *output, uint32_t msecs)
{
	struct wlsc_compositor *compositor =
		container_of(animation,
			     struct wlsc_compositor, fade.animation);

	wlsc_spring_update(&compositor->fade.spring, msecs);
	if (wlsc_spring_done(&compositor->fade.spring)) {
		if (compositor->fade.spring.current > 0.999) {
			compositor->state = WLSC_COMPOSITOR_SLEEPING;
			compositor->shell->lock(compositor->shell);
		}
		compositor->fade.spring.current =
			compositor->fade.spring.target;
		wl_list_remove(&animation->link);
		wl_list_init(&animation->link);
	}

	wlsc_output_damage(output);
}

static void
fade_output(struct wlsc_output *output,
	    GLfloat tint, pixman_region32_t *region)
{
	struct wlsc_compositor *compositor = output->compositor;
	struct wlsc_surface surface;
	GLfloat color[4] = { 0.0, 0.0, 0.0, tint };

	surface.compositor = compositor;
	surface.x = output->x;
	surface.y = output->y;
	surface.pitch = output->current->width;
	surface.width = output->current->width;
	surface.height = output->current->height;
	surface.texture = GL_NONE;
	surface.transform = NULL;

	if (tint <= 1.0)
		surface.visual =
			&compositor->compositor.premultiplied_argb_visual;
	else
		surface.visual = &compositor->compositor.rgb_visual;

	glUseProgram(compositor->solid_shader.program);
	glUniformMatrix4fv(compositor->solid_shader.proj_uniform,
			   1, GL_FALSE, output->matrix.d);
	glUniform4fv(compositor->solid_shader.color_uniform, 1, color);
	wlsc_surface_draw(&surface, output, region);
}

static void
wlsc_output_set_cursor(struct wlsc_output *output,
		       struct wl_input_device *dev, int force_sw)
{
	struct wlsc_compositor *ec = output->compositor;
	struct wlsc_input_device *device = (struct wlsc_input_device *) dev;
	pixman_region32_t cursor_region;
	int use_hardware_cursor = 1, prior_was_hardware;

	pixman_region32_init_rect(&cursor_region,
				  device->sprite->x, device->sprite->y,
				  device->sprite->width,
				  device->sprite->height);

	pixman_region32_intersect(&cursor_region, &cursor_region, &output->region);

	if (!pixman_region32_not_empty(&cursor_region)) {
 		output->set_hardware_cursor(output, NULL);
		goto out;
	}

	prior_was_hardware = wl_list_empty(&device->sprite->link);
	if (force_sw || output->set_hardware_cursor(output, device) < 0) {
		if (prior_was_hardware)
			wlsc_surface_damage(device->sprite);
		use_hardware_cursor = 0;
	} else if (!prior_was_hardware) {
		wlsc_surface_damage_below(device->sprite);
	}

	/* Remove always to be on top. */
	wl_list_remove(&device->sprite->link);
	if (!use_hardware_cursor)
		wl_list_insert(&ec->surface_list, &device->sprite->link);
	else
		wl_list_init(&device->sprite->link);

out:
	pixman_region32_fini(&cursor_region);
}

static void
wlsc_output_repaint(struct wlsc_output *output)
{
	struct wlsc_compositor *ec = output->compositor;
	struct wlsc_surface *es;
	pixman_region32_t opaque, new_damage, total_damage, repaint;

	output->prepare_render(output);

	glViewport(0, 0, output->current->width, output->current->height);

	glUseProgram(ec->texture_shader.program);
	glUniformMatrix4fv(ec->texture_shader.proj_uniform,
			   1, GL_FALSE, output->matrix.d);
	glUniform1i(ec->texture_shader.tex_uniform, 0);

	wlsc_output_set_cursor(output, ec->input_device,
			       !(ec->focus && ec->fade.spring.current < 0.001));

	pixman_region32_init(&new_damage);
	pixman_region32_copy(&new_damage, &ec->damage);
	pixman_region32_init(&opaque);
				
	wl_list_for_each(es, &ec->surface_list, link) {
		pixman_region32_subtract(&es->damage, &es->damage, &opaque);
		pixman_region32_union(&new_damage, &new_damage, &es->damage);
		pixman_region32_union(&opaque, &opaque, &es->opaque);
	}

	pixman_region32_subtract(&ec->damage, &ec->damage, &output->region);

	pixman_region32_init(&total_damage);
	pixman_region32_union(&total_damage, &new_damage,
			      &output->previous_damage);
	pixman_region32_intersect(&output->previous_damage,
				  &new_damage, &output->region);

	pixman_region32_fini(&opaque);
	pixman_region32_fini(&new_damage);

	es = container_of(ec->surface_list.next, struct wlsc_surface, link);

	if (es->visual == &ec->compositor.rgb_visual &&
	    output->prepare_scanout_surface(output, es) == 0) {
		/* We're drawing nothing now,
		 * draw the damaged regions later. */
		pixman_region32_union(&ec->damage, &ec->damage, &total_damage);

		output->scanout_buffer = es->buffer;
		output->scanout_buffer->busy_count++;

		wl_list_remove(&output->scanout_buffer_destroy_listener.link);
		wl_list_insert(output->scanout_buffer->resource.destroy_listener_list.prev,
			       &output->scanout_buffer_destroy_listener.link);

		return;
	}

	if (es->fullscreen_output == output) {
		if (es->width < output->current->width ||
		    es->height < output->current->height)
			glClear(GL_COLOR_BUFFER_BIT);
		wlsc_surface_draw(es, output, &total_damage);
	} else {
		wl_list_for_each(es, &ec->surface_list, link) {
			pixman_region32_copy(&es->damage, &total_damage);
			pixman_region32_subtract(&total_damage, &total_damage, &es->opaque);
		}

		wl_list_for_each_reverse(es, &ec->surface_list, link) {
			pixman_region32_init(&repaint);
			pixman_region32_intersect(&repaint, &output->region,
						  &es->damage);
			wlsc_surface_draw(es, output, &repaint);
			pixman_region32_subtract(&es->damage,
						 &es->damage, &output->region);
		}
	}

	if (ec->fade.spring.current > 0.001)
		fade_output(output, ec->fade.spring.current, &total_damage);
}

static void
repaint(void *data, int msecs)
{
	struct wlsc_output *output = data;
	struct wlsc_compositor *compositor = output->compositor;
	struct wlsc_surface *es;
	struct wlsc_animation *animation, *next;

	wlsc_output_repaint(output);
	output->repaint_needed = 0;
	output->repaint_scheduled = 1;
	output->present(output);

	/* FIXME: Keep the surfaces in an per-output list. */
	wl_list_for_each(es, &compositor->surface_list, link) {
		if (es->output == output) {
			wl_display_post_frame(compositor->wl_display,
					      &es->surface, msecs);
		}
	}

	wl_list_for_each_safe(animation, next,
			      &compositor->animation_list, link)
		animation->frame(animation, output, msecs);
}

static void
idle_repaint(void *data)
{
	repaint(data, wlsc_compositor_get_time());
}

WL_EXPORT void
wlsc_output_finish_frame(struct wlsc_output *output, int msecs)
{
	wlsc_buffer_post_release(output->scanout_buffer);
	output->scanout_buffer = NULL;
	output->repaint_scheduled = 0;

	if (output->repaint_needed)
		repaint(output, msecs);
}

WL_EXPORT void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor)
{
	struct wlsc_output *output;
	struct wl_event_loop *loop;

	if (compositor->state == WLSC_COMPOSITOR_SLEEPING)
		return;

	loop = wl_display_get_event_loop(compositor->wl_display);
	wl_list_for_each(output, &compositor->output_list, link) {
		output->repaint_needed = 1;
		if (output->repaint_scheduled)
			continue;

		wl_event_loop_add_idle(loop, idle_repaint, output);
		output->repaint_scheduled = 1;
	}
}

WL_EXPORT void
wlsc_compositor_fade(struct wlsc_compositor *compositor, float tint)
{
	int done;

	done = wlsc_spring_done(&compositor->fade.spring);
	compositor->fade.spring.target = tint;
	if (wlsc_spring_done(&compositor->fade.spring))
		return;

	if (done)
		compositor->fade.spring.timestamp =
			wlsc_compositor_get_time();

	wlsc_compositor_damage_all(compositor);
	if (wl_list_empty(&compositor->fade.animation.link))
		wl_list_insert(compositor->animation_list.prev,
			       &compositor->fade.animation.link);
}

static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	wl_resource_destroy(&surface->resource, client,
			    wlsc_compositor_get_time());
}

WL_EXPORT void
wlsc_surface_assign_output(struct wlsc_surface *es)
{
	struct wlsc_compositor *ec = es->compositor;
	struct wlsc_output *output;
	pixman_region32_t region;
	uint32_t max, area;
	pixman_box32_t *e;

	max = 0;
	wl_list_for_each(output, &ec->output_list, link) {
		pixman_region32_init_rect(&region,
					  es->x, es->y, es->width, es->height);
		pixman_region32_intersect(&region, &region, &output->region);

		e = pixman_region32_extents(&region);
		area = (e->x2 - e->x1) * (e->y2 - e->y1);

		if (area >= max) {
			es->output = output;
			max = area;
		}
	}
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, struct wl_buffer *buffer,
	       int32_t x, int32_t y)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	buffer->busy_count++;
	wlsc_buffer_post_release(es->buffer);

	es->buffer = buffer;
	wl_list_remove(&es->buffer_destroy_listener.link);
	wl_list_insert(es->buffer->resource.destroy_listener_list.prev,
		       &es->buffer_destroy_listener.link);

	if (es->visual == NULL)
		wl_list_insert(&es->compositor->surface_list, &es->link);

	es->visual = buffer->visual;
	if (x != 0 || y != 0 ||
	    es->width != buffer->width || es->height != buffer->height)
		wlsc_surface_configure(es, es->x + x, es->y + y,
				       buffer->width, buffer->height);

	wlsc_buffer_attach(buffer, surface);

	es->compositor->shell->attach(es->compositor->shell, es);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	wlsc_surface_damage_rectangle(es, x, y, width, height);
}

const static struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_damage
};

static void
wlsc_input_device_attach(struct wlsc_input_device *device,
			 int x, int y, int width, int height)
{
	wlsc_surface_damage_below(device->sprite);

	device->hotspot_x = x;
	device->hotspot_y = y;

	device->sprite->x = device->input_device.x - device->hotspot_x;
	device->sprite->y = device->input_device.y - device->hotspot_y;
	device->sprite->width = width;
	device->sprite->height = height;

	wlsc_surface_damage(device->sprite);
}

static void
wlsc_input_device_attach_buffer(struct wlsc_input_device *device,
				struct wl_buffer *buffer, int x, int y)
{
	wlsc_buffer_attach(buffer, &device->sprite->surface);
	wlsc_input_device_attach(device, x, y, buffer->width, buffer->height);
}

static void
wlsc_input_device_attach_sprite(struct wlsc_input_device *device,
				struct wlsc_sprite *sprite, int x, int y)
{
	wlsc_sprite_attach(sprite, &device->sprite->surface);
	wlsc_input_device_attach(device, x, y, sprite->width, sprite->height);
}

WL_EXPORT void
wlsc_input_device_set_pointer_image(struct wlsc_input_device *device,
				    enum wlsc_pointer_type type)
{
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->input_device.compositor;

	wlsc_input_device_attach_sprite(device,
					compositor->pointer_sprites[type],
					pointer_images[type].hotspot_x,
					pointer_images[type].hotspot_y);
}

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	struct wlsc_compositor *ec = (struct wlsc_compositor *) compositor;
	struct wlsc_surface *surface;

	surface = wlsc_surface_create(ec, 0, 0, 0, 0);
	if (surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	surface->surface.resource.destroy = destroy_surface;

	surface->surface.resource.object.id = id;
	surface->surface.resource.object.interface = &wl_surface_interface;
	surface->surface.resource.object.implementation =
		(void (**)(void)) &surface_interface;
	surface->surface.client = client;

	wl_client_add_resource(client, &surface->surface.resource);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
};

static void
wlsc_surface_transform(struct wlsc_surface *surface,
		       int32_t x, int32_t y, int32_t *sx, int32_t *sy)
{
	*sx = x - surface->x;
	*sy = y - surface->y;
}

WL_EXPORT struct wlsc_surface *
pick_surface(struct wl_input_device *device, int32_t *sx, int32_t *sy)
{
	struct wlsc_compositor *ec =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_surface *es;

	wl_list_for_each(es, &ec->surface_list, link) {
		if (es->surface.client == NULL)
			continue;
		wlsc_surface_transform(es, device->x, device->y, sx, sy);
		if (0 <= *sx && *sx < es->width &&
		    0 <= *sy && *sy < es->height)
			return es;
	}

	return NULL;
}


static void
motion_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) grab->input_device;
	struct wlsc_surface *es =
		(struct wlsc_surface *) device->input_device.pointer_focus;
	int32_t sx, sy;

	wlsc_surface_transform(es, x, y, &sx, &sy);
	wl_client_post_event(es->surface.client,
			     &device->input_device.object,
			     WL_INPUT_DEVICE_MOTION,
			     time, x, y, sx, sy);
}

static void
motion_grab_button(struct wl_grab *grab,
		   uint32_t time, int32_t button, int32_t state)
{
	wl_client_post_event(grab->input_device->pointer_focus->client,
			     &grab->input_device->object,
			     WL_INPUT_DEVICE_BUTTON,
			     time, button, state);
}

static void
motion_grab_end(struct wl_grab *grab, uint32_t time)
{
}

static const struct wl_grab_interface motion_grab_interface = {
	motion_grab_motion,
	motion_grab_button,
	motion_grab_end
};

WL_EXPORT void
wlsc_compositor_wake(struct wlsc_compositor *compositor)
{
	if (compositor->idle_inhibit)
		return;

	wlsc_compositor_fade(compositor, 0.0);
	compositor->state = WLSC_COMPOSITOR_ACTIVE;

	wl_event_source_timer_update(compositor->idle_source,
				     option_idle_time * 1000);
}

static void
wlsc_compositor_idle_inhibit(struct wlsc_compositor *compositor)
{
	wlsc_compositor_wake(compositor);
	compositor->idle_inhibit++;
}

static void
wlsc_compositor_idle_release(struct wlsc_compositor *compositor)
{
	compositor->idle_inhibit--;
	wlsc_compositor_wake(compositor);
}

static int
idle_handler(void *data)
{
	struct wlsc_compositor *compositor = data;

	if (compositor->idle_inhibit)
		return 1;

	wlsc_compositor_fade(compositor, 1.0);

	return 1;
}

WL_EXPORT void
notify_motion(struct wl_input_device *device, uint32_t time, int x, int y)
{
	struct wlsc_surface *es;
	struct wlsc_compositor *ec =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_output *output;
	const struct wl_grab_interface *interface;
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	int32_t sx, sy;
	int x_valid = 0, y_valid = 0;
	int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;

	wlsc_compositor_wake(ec);

	wl_list_for_each(output, &ec->output_list, link) {
		if (output->x <= x && x <= output->x + output->current->width)
			x_valid = 1;

		if (output->y <= y && y <= output->y + output->current->height)
			y_valid = 1;

		/* FIXME: calculate this only on output addition/deletion */
		if (output->x < min_x)
			min_x = output->x;
		if (output->y < min_y)
			min_y = output->y;

		if (output->x + output->current->width > max_x)
			max_x = output->x + output->current->width;
		if (output->y + output->current->height > max_y)
			max_y = output->y + output->current->height;
	}
	
	if (!x_valid) {
		if (x < min_x)
			x = min_x;
		else if (x >= max_x)
			x = max_x;
	}
	if (!y_valid) {
		if (y < min_y)
			y = min_y;
		else  if (y >= max_y)
			y = max_y;
	}

	device->x = x;
	device->y = y;

	if (device->grab) {
		interface = device->grab->interface;
		interface->motion(device->grab, time, x, y);
	} else {
		es = pick_surface(device, &sx, &sy);
		wl_input_device_set_pointer_focus(device,
						  &es->surface,
						  time, x, y, sx, sy);
		if (es)
			wl_client_post_event(es->surface.client,
					     &device->object,
					     WL_INPUT_DEVICE_MOTION,
					     time, x, y, sx, sy);
	}

	wlsc_surface_damage_below(wd->sprite);

	wd->sprite->x = device->x - wd->hotspot_x;
	wd->sprite->y = device->y - wd->hotspot_y;

	wlsc_surface_damage(wd->sprite);
}

WL_EXPORT void
wlsc_surface_activate(struct wlsc_surface *surface,
		      struct wlsc_input_device *device, uint32_t time)
{
	struct wlsc_shell *shell = surface->compositor->shell;

	wlsc_surface_raise(surface);
	if (device->selection)
		shell->set_selection_focus(shell,
					   device->selection,
					   &surface->surface, time);

	wl_input_device_set_keyboard_focus(&device->input_device,
					   &surface->surface,
					   time);
}

struct wlsc_binding {
	uint32_t key;
	uint32_t button;
	uint32_t modifier;
	wlsc_binding_handler_t handler;
	void *data;
	struct wl_list link;
};

WL_EXPORT void
notify_button(struct wl_input_device *device,
	      uint32_t time, int32_t button, int32_t state)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_binding *b;
	struct wlsc_surface *surface =
		(struct wlsc_surface *) device->pointer_focus;

	if (state)
		wlsc_compositor_idle_inhibit(compositor);
	else
		wlsc_compositor_idle_release(compositor);

	if (state && surface && device->grab == NULL) {
		wlsc_surface_activate(surface, wd, time);
		wl_input_device_start_grab(device,
					   &device->motion_grab,
					   button, time);
	}

	wl_list_for_each(b, &compositor->binding_list, link) {
		if (b->button == button &&
		    b->modifier == wd->modifier_state && state) {
			b->handler(&wd->input_device,
				   time, 0, button, state, b->data);
			break;
		}
	}

	if (device->grab)
		device->grab->interface->button(device->grab, time,
						button, state);

	if (!state && device->grab && device->grab_button == button)
		wl_input_device_end_grab(device, time);
}

static void
terminate_binding(struct wl_input_device *device, uint32_t time,
		  uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct wlsc_compositor *compositor = data;

	if (state)
		wl_display_terminate(compositor->wl_display);
}

WL_EXPORT struct wlsc_binding *
wlsc_compositor_add_binding(struct wlsc_compositor *compositor,
			    uint32_t key, uint32_t button, uint32_t modifier,
			    wlsc_binding_handler_t handler, void *data)
{
	struct wlsc_binding *binding;

	binding = malloc(sizeof *binding);
	if (binding == NULL)
		return NULL;

	binding->key = key;
	binding->button = button;
	binding->modifier = modifier;
	binding->handler = handler;
	binding->data = data;
	wl_list_insert(compositor->binding_list.prev, &binding->link);

	return binding;
}

WL_EXPORT void
wlsc_binding_destroy(struct wlsc_binding *binding)
{
	wl_list_remove(&binding->link);
	free(binding);
}

static void
update_modifier_state(struct wlsc_input_device *device,
		      uint32_t key, uint32_t state)
{
	uint32_t modifier;

	switch (key) {
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		modifier = MODIFIER_CTRL;
		break;

	case KEY_LEFTALT:
	case KEY_RIGHTALT:
		modifier = MODIFIER_ALT;
		break;

	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		modifier = MODIFIER_SUPER;
		break;

	default:
		modifier = 0;
		break;
	}

	if (state)
		device->modifier_state |= modifier;
	else
		device->modifier_state &= ~modifier;
}

WL_EXPORT void
notify_key(struct wl_input_device *device,
	   uint32_t time, uint32_t key, uint32_t state)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	uint32_t *k, *end;
	struct wlsc_binding *b;

	if (state)
		wlsc_compositor_idle_inhibit(compositor);
	else
		wlsc_compositor_idle_release(compositor);

	wl_list_for_each(b, &compositor->binding_list, link) {
		if (b->key == key &&
		    b->modifier == wd->modifier_state) {
			b->handler(&wd->input_device,
				   time, key, 0, state, b->data);
			break;
		}
	}

	update_modifier_state(wd, key, state);
	end = device->keys.data + device->keys.size;
	for (k = device->keys.data; k < end; k++) {
		if (*k == key)
			*k = *--end;
	}
	device->keys.size = (void *) end - device->keys.data;
	if (state) {
		k = wl_array_add(&device->keys, sizeof *k);
		*k = key;
	}

	if (device->keyboard_focus != NULL)
		wl_client_post_event(device->keyboard_focus->client,
				     &device->object,
				     WL_INPUT_DEVICE_KEY, time, key, state);
}

WL_EXPORT void
notify_pointer_focus(struct wl_input_device *device,
		     uint32_t time, struct wlsc_output *output,
		     int32_t x, int32_t y)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_surface *es;
	int32_t sx, sy;

	if (output) {
		device->x = x;
		device->y = y;
		es = pick_surface(device, &sx, &sy);
		wl_input_device_set_pointer_focus(device,
						  &es->surface,
						  time, x, y, sx, sy);

		compositor->focus = 1;

		wd->sprite->x = device->x - wd->hotspot_x;
		wd->sprite->y = device->y - wd->hotspot_y;
	} else {
		wl_input_device_set_pointer_focus(device, NULL,
						  time, 0, 0, 0, 0);
		compositor->focus = 0;
	}

	wlsc_surface_damage(wd->sprite);
}

WL_EXPORT void
notify_keyboard_focus(struct wl_input_device *device,
		      uint32_t time, struct wlsc_output *output,
		      struct wl_array *keys)
{
	struct wlsc_input_device *wd =
		(struct wlsc_input_device *) device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) device->compositor;
	struct wlsc_surface *es;
	uint32_t *k, *end;

	if (!wl_list_empty(&compositor->surface_list))
		es = container_of(compositor->surface_list.next,
				  struct wlsc_surface, link);
	else
		es = NULL;

	if (output) {
		wl_array_copy(&wd->input_device.keys, keys);
		wd->modifier_state = 0;
		end = device->keys.data + device->keys.size;
		for (k = device->keys.data; k < end; k++) {
			wlsc_compositor_idle_inhibit(compositor);
			update_modifier_state(wd, *k, 1);
		}

		if (es->surface.client)
			wl_input_device_set_keyboard_focus(&wd->input_device,
							   &es->surface, time);
	} else {
		end = device->keys.data + device->keys.size;
		for (k = device->keys.data; k < end; k++)
			wlsc_compositor_idle_release(compositor);

		wd->modifier_state = 0;
		wl_input_device_set_keyboard_focus(&wd->input_device,
						   NULL, time);
	}
}


static void
input_device_attach(struct wl_client *client,
		    struct wl_input_device *device_base,
		    uint32_t time,
		    struct wl_buffer *buffer, int32_t x, int32_t y)
{
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) device_base;

	if (time < device->input_device.pointer_focus_time)
		return;
	if (device->input_device.pointer_focus == NULL)
		return;
	if (device->input_device.pointer_focus->client != client)
		return;

	if (buffer == NULL) {
		wlsc_input_device_set_pointer_image(device,
						    WLSC_POINTER_LEFT_PTR);
		return;
	}

	wlsc_input_device_attach_buffer(device, buffer, x, y);
}

const static struct wl_input_device_interface input_device_interface = {
	input_device_attach,
};

WL_EXPORT void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec)
{
	wl_input_device_init(&device->input_device, &ec->compositor);

	device->input_device.object.interface = &wl_input_device_interface;
	device->input_device.object.implementation =
		(void (**)(void)) &input_device_interface;
	wl_display_add_object(ec->wl_display, &device->input_device.object);
	wl_display_add_global(ec->wl_display, &device->input_device.object, NULL);

	device->sprite = wlsc_surface_create(ec,
					     device->input_device.x,
					     device->input_device.y, 32, 32);
	wl_list_insert(&ec->surface_list, &device->sprite->link);

	device->hotspot_x = 16;
	device->hotspot_y = 16;
	device->modifier_state = 0;

	device->input_device.motion_grab.interface = &motion_grab_interface;

	wl_list_insert(ec->input_device_list.prev, &device->link);

	wlsc_input_device_set_pointer_image(device, WLSC_POINTER_LEFT_PTR);
}

static void
wlsc_output_post_geometry(struct wl_client *client,
			  struct wl_object *global, uint32_t version)
{
	struct wlsc_output *output =
		container_of(global, struct wlsc_output, object);
	struct wlsc_mode *mode;

	wl_client_post_event(client, global,
			     WL_OUTPUT_GEOMETRY,
			     output->x,
			     output->y,
			     output->mm_width,
			     output->mm_height,
			     output->subpixel,
			     output->make, output->model);

	wl_list_for_each (mode, &output->mode_list, link) {
		wl_client_post_event(client, global,
				     WL_OUTPUT_MODE,
				     mode->flags,
				     mode->width, mode->height, mode->refresh);
	}
}

static const char vertex_shader[] =
	"uniform mat4 proj;\n"
	"attribute vec2 position;\n"
	"attribute vec2 texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = proj * vec4(position, 0.0, 1.0);\n"
	"   v_texcoord = texcoord;\n"
	"}\n";

static const char texture_fragment_shader[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = texture2D(tex, v_texcoord)\n;"
	"}\n";

static const char solid_fragment_shader[] =
	"precision mediump float;\n"
	"uniform vec4 color;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = color\n;"
	"}\n";

static int
compile_shader(GLenum type, const char *source)
{
	GLuint s;
	char msg[512];
	GLint status;

	s = glCreateShader(type);
	glShaderSource(s, 1, &source, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(s, sizeof msg, NULL, msg);
		fprintf(stderr, "shader info: %s\n", msg);
		return GL_NONE;
	}

	return s;
}

static int
wlsc_shader_init(struct wlsc_shader *shader,
		 const char *vertex_source, const char *fragment_source)
{
	char msg[512];
	GLint status;

	shader->vertex_shader =
		compile_shader(GL_VERTEX_SHADER, vertex_source);
	shader->fragment_shader =
		compile_shader(GL_FRAGMENT_SHADER, fragment_source);

	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader->vertex_shader);
	glAttachShader(shader->program, shader->fragment_shader);
	glBindAttribLocation(shader->program, 0, "position");
	glBindAttribLocation(shader->program, 1, "texcoord");

	glLinkProgram(shader->program);
	glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(shader->program, sizeof msg, NULL, msg);
		fprintf(stderr, "link info: %s\n", msg);
		return -1;
	}

	shader->proj_uniform = glGetUniformLocation(shader->program, "proj");
	shader->tex_uniform = glGetUniformLocation(shader->program, "tex");

	return 0;
}

static int
init_solid_shader(struct wlsc_shader *shader,
		  GLuint vertex_shader, const char *fragment_source)
{
	GLint status;
	char msg[512];

	shader->vertex_shader = vertex_shader;
	shader->fragment_shader =
		compile_shader(GL_FRAGMENT_SHADER, fragment_source);

	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader->vertex_shader);
	glAttachShader(shader->program, shader->fragment_shader);
	glBindAttribLocation(shader->program, 0, "position");
	glBindAttribLocation(shader->program, 1, "texcoord");

	glLinkProgram(shader->program);
	glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(shader->program, sizeof msg, NULL, msg);
 		fprintf(stderr, "link info: %s\n", msg);
		return -1;
 	}
 
	shader->proj_uniform = glGetUniformLocation(shader->program, "proj");
	shader->color_uniform = glGetUniformLocation(shader->program, "color");

	return 0;
}

WL_EXPORT void
wlsc_output_destroy(struct wlsc_output *output)
{
	pixman_region32_fini(&output->region);
	pixman_region32_fini(&output->previous_damage);
	destroy_surface(&output->background->surface.resource, NULL);
}

WL_EXPORT void
wlsc_output_move(struct wlsc_output *output, int x, int y)
{
	struct wlsc_compositor *c = output->compositor;
	int flip;

	output->x = x;
	output->y = y;

	if (output->background) {
		output->background->x = x;
		output->background->y = y;
	}

	pixman_region32_init(&output->previous_damage);
	pixman_region32_init_rect(&output->region, x, y, 
				  output->current->width,
				  output->current->height);

	wlsc_matrix_init(&output->matrix);
	wlsc_matrix_translate(&output->matrix,
			      -output->x - output->current->width / 2.0,
			      -output->y - output->current->height / 2.0, 0);

	flip = (output->flags & WL_OUTPUT_FLIPPED) ? -1 : 1;
	wlsc_matrix_scale(&output->matrix,
			  2.0 / output->current->width,
			  flip * 2.0 / output->current->height, 1);

	pixman_region32_union(&c->damage, &c->damage, &output->region);
}

WL_EXPORT void
wlsc_output_init(struct wlsc_output *output, struct wlsc_compositor *c,
		 int x, int y, int width, int height, uint32_t flags)
{
	output->compositor = c;
	output->x = x;
	output->y = y;
	output->mm_width = width;
	output->mm_height = height;

	output->background =
		background_create(output, option_background);
 
	if (output->background != NULL)
		wl_list_insert(c->surface_list.prev,
			       &output->background->link);

	output->flags = flags;
	wlsc_output_move(output, x, y);

	output->scanout_buffer_destroy_listener.func =
		output_handle_scanout_buffer_destroy;
	wl_list_init(&output->scanout_buffer_destroy_listener.link);

	output->object.interface = &wl_output_interface;
	wl_display_add_object(c->wl_display, &output->object);
	wl_display_add_global(c->wl_display, &output->object,
			      wlsc_output_post_geometry);
}

static void
shm_buffer_created(struct wl_buffer *buffer)
{
	struct wl_list *surfaces_attached_to;

	surfaces_attached_to = malloc(sizeof *surfaces_attached_to);
	if (!surfaces_attached_to) {
		buffer->user_data = NULL;
		return;
	}

	wl_list_init(surfaces_attached_to);

	buffer->user_data = surfaces_attached_to;
}

static void
shm_buffer_damaged(struct wl_buffer *buffer,
		   int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wl_list *surfaces_attached_to = buffer->user_data;
	struct wlsc_surface *es;
	GLsizei tex_width = wl_shm_buffer_get_stride(buffer) / 4;

	wl_list_for_each(es, surfaces_attached_to, buffer_link) {
		glBindTexture(GL_TEXTURE_2D, es->texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     tex_width, buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(buffer));
		/* Hmm, should use glTexSubImage2D() here but GLES2 doesn't
		 * support any unpack attributes except GL_UNPACK_ALIGNMENT. */
	}
}

static void
shm_buffer_destroyed(struct wl_buffer *buffer)
{
	struct wl_list *surfaces_attached_to = buffer->user_data;
	struct wlsc_surface *es, *next;

	wl_list_for_each_safe(es, next, surfaces_attached_to, buffer_link) {
		wl_list_remove(&es->buffer_link);
		wl_list_init(&es->buffer_link);
	}

	free(surfaces_attached_to);
}

const static struct wl_shm_callbacks shm_callbacks = {
	shm_buffer_created,
	shm_buffer_damaged,
	shm_buffer_destroyed
};

WL_EXPORT int
wlsc_compositor_init(struct wlsc_compositor *ec, struct wl_display *display)
{
	struct wl_event_loop *loop;
	const char *extensions;

	ec->wl_display = display;

	wl_compositor_init(&ec->compositor, &compositor_interface, display);

	ec->shm = wl_shm_init(display, &shm_callbacks);

	ec->image_target_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	ec->image_target_renderbuffer_storage = (void *)
		eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	ec->create_image = (void *) eglGetProcAddress("eglCreateImageKHR");
	ec->destroy_image = (void *) eglGetProcAddress("eglDestroyImageKHR");
	ec->bind_display =
		(void *) eglGetProcAddress("eglBindWaylandDisplayWL");
	ec->unbind_display =
		(void *) eglGetProcAddress("eglUnbindWaylandDisplayWL");

	extensions = (const char *) glGetString(GL_EXTENSIONS);
	if (!strstr(extensions, "GL_EXT_texture_format_BGRA8888")) {
		fprintf(stderr,
			"GL_EXT_texture_format_BGRA8888 not available\n");
		return -1;
	}

	extensions =
		(const char *) eglQueryString(ec->display, EGL_EXTENSIONS);
	if (strstr(extensions, "EGL_WL_bind_wayland_display"))
		ec->has_bind_display = 1;
	if (ec->has_bind_display)
		ec->bind_display(ec->display, ec->wl_display);

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->input_device_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->binding_list);
	wl_list_init(&ec->animation_list);
	wlsc_spring_init(&ec->fade.spring, 0.8, 0.0, 0.0);
	ec->fade.animation.frame = fade_frame;
	wl_list_init(&ec->fade.animation.link);

	wlsc_compositor_add_binding(ec, KEY_BACKSPACE, 0,
				    MODIFIER_CTRL | MODIFIER_ALT,
				    terminate_binding, ec);

	create_pointer_images(ec);

	screenshooter_create(ec);

	glActiveTexture(GL_TEXTURE0);

	if (wlsc_shader_init(&ec->texture_shader,
			     vertex_shader, texture_fragment_shader) < 0)
		return -1;
	if (init_solid_shader(&ec->solid_shader,
			      ec->texture_shader.vertex_shader,
			      solid_fragment_shader) < 0)
		return -1;

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->idle_source = wl_event_loop_add_timer(loop, idle_handler, ec);
	wl_event_source_timer_update(ec->idle_source, option_idle_time * 1000);

	pixman_region32_init(&ec->damage);
	wlsc_compositor_schedule_repaint(ec);

	return 0;
}

WL_EXPORT int
wlsc_compositor_shutdown(struct wlsc_compositor *ec)
{
	struct wlsc_output *output;

	/* Destroy all outputs associated with this compositor */
	wl_list_for_each(output, &ec->output_list, link)
		output->destroy(output);
}

static int on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	fprintf(stderr, "caught signal %d\n", signal_number);
	wl_display_terminate(display);

	return 1;
}

static void *
load_module(const char *name, const char *entrypoint, void **handle)
{
	char path[PATH_MAX];
	void *module, *init;

	if (name[0] != '/')
		snprintf(path, sizeof path, MODULEDIR "/%s", name);
	else
		snprintf(path, sizeof path, "%s", name);

	module = dlopen(path, RTLD_LAZY);
	if (!module) {
		fprintf(stderr,
			"failed to load module: %s\n", dlerror());
		return NULL;
	}

	init = dlsym(module, entrypoint);
	if (!init) {
		fprintf(stderr,
			"failed to lookup init function: %s\n", dlerror());
		return NULL;
	}

	return init;
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wlsc_compositor *ec;
	struct wl_event_loop *loop;
	int o, xserver = 0;
	void *shell_module, *backend_module;
	int (*shell_init)(struct wlsc_compositor *ec);
	struct wlsc_compositor
		*(*backend_init)(struct wl_display *display, char *options);
	char *backend = NULL;
	char *backend_options = "";
	char *shell = NULL;
	char *p;

	static const char opts[] = "B:b:o:S:i:s:x";
	static const struct option longopts[ ] = {
		{ "backend", 1, NULL, 'B' },
		{ "backend-options", 1, NULL, 'o' },
		{ "background", 1, NULL, 'b' },
		{ "socket", 1, NULL, 'S' },
		{ "idle-time", 1, NULL, 'i' },
		{ "shell", 1, NULL, 's' },
		{ "xserver", 0, NULL, 'x' },
		{ NULL, }
	};

	while (o = getopt_long(argc, argv, opts, longopts, &o), o > 0) {
		switch (o) {
		case 'b':
			option_background = optarg;
			break;
		case 'B':
			backend = optarg;
			break;
		case 'o':
			backend_options = optarg;
			break;
		case 'S':
			option_socket_name = optarg;
			break;
		case 'i':
			option_idle_time = strtol(optarg, &p, 0);
			if (*p != '\0') {
				fprintf(stderr,
					"invalid idle time option: %s\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 's':
			shell = optarg;
			break;
		case 'x':
			xserver = 1;
			break;
		}
	}

	display = wl_display_create();

	loop = wl_display_get_event_loop(display);
	wl_event_loop_add_signal(loop, SIGTERM, on_term_signal, display);
	wl_event_loop_add_signal(loop, SIGINT, on_term_signal, display);
	wl_event_loop_add_signal(loop, SIGQUIT, on_term_signal, display);

	wl_list_init(&child_process_list);
	wl_event_loop_add_signal(loop, SIGCHLD, sigchld_handler, NULL);

	if (!backend) {
		if (getenv("WAYLAND_DISPLAY"))
			backend = "wayland-backend.so";
		else if (getenv("DISPLAY"))
			backend = "x11-backend.so";
		else if (getenv("OPENWFD"))
			backend = "openwfd-backend.so";
		else
			backend = "drm-backend.so";
	}

	if (!shell)
		shell = "desktop-shell.so";

	backend_init = load_module(backend, "backend_init", &backend_module);
	if (!backend_init)
		exit(EXIT_FAILURE);

	shell_init = load_module(shell, "shell_init", &shell_module);
	if (!shell_init)
		exit(EXIT_FAILURE);

	ec = backend_init(display, backend_options);
	if (ec == NULL) {
		fprintf(stderr, "failed to create compositor\n");
		exit(EXIT_FAILURE);
	}

	if (shell_init(ec) < 0)
		exit(EXIT_FAILURE);

	if (xserver)
		wlsc_xserver_init(ec);

	if (wl_display_add_socket(display, option_socket_name)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	wl_display_run(display);

	if (xserver)
		wlsc_xserver_destroy(ec);

	if (ec->has_bind_display)
		ec->unbind_display(ec->display, display);
	wl_display_destroy(display);

	ec->destroy(ec);

	return 0;
}

/*
 * Copyright © 2012 Collabora, Ltd.
 * Copyright © 2012 Rob Clark
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

/* cliptest: for debugging calculate_edges() function, which is copied
 * from compositor.c.
 * controls:
 *	clip box position: mouse left drag, keys: w a s d
 *	clip box size: mouse right drag, keys: i j k l
 *	surface orientation: mouse wheel, keys: n m
 *	surface transform disable key: r
 */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <pixman.h>
#include <cairo.h>
#include <float.h>
#include <assert.h>

#include <linux/input.h>
#include <wayland-client.h>

#include "window.h"

typedef float GLfloat;

struct geometry {
	pixman_box32_t clip;

	pixman_box32_t surf;
	float s; /* sin phi */
	float c; /* cos phi */
	float phi;
};

struct weston_surface {
	struct {
		int enabled;
	} transform;

	struct geometry *geometry;
};

static void
weston_surface_to_global_float(struct weston_surface *surface,
			       GLfloat sx, GLfloat sy, GLfloat *x, GLfloat *y)
{
	struct geometry *g = surface->geometry;

	/* pure rotation around origin by sine and cosine */
	*x = g->c * sx + g->s * sy;
	*y = -g->s * sx + g->c * sy;
}

/* ---------------------- copied begins -----------------------*/

struct polygon8 {
	GLfloat x[8];
	GLfloat y[8];
	int n;
};

struct clip_context {
	struct {
		GLfloat x;
		GLfloat y;
	} prev;

	struct {
		GLfloat x1, y1;
		GLfloat x2, y2;
	} clip;

	struct {
		GLfloat *x;
		GLfloat *y;
	} vertices;
};

static GLfloat
float_difference(GLfloat a, GLfloat b)
{
	/* http://www.altdevblogaday.com/2012/02/22/comparing-floating-point-numbers-2012-edition/ */
	static const GLfloat max_diff = 4.0f * FLT_MIN;
	static const GLfloat max_rel_diff = 4.0e-5;
	GLfloat diff = a - b;
	GLfloat adiff = fabsf(diff);

	if (adiff <= max_diff)
		return 0.0f;

	a = fabsf(a);
	b = fabsf(b);
	if (adiff <= (a > b ? a : b) * max_rel_diff)
		return 0.0f;

	return diff;
}

/* A line segment (p1x, p1y)-(p2x, p2y) intersects the line x = x_arg.
 * Compute the y coordinate of the intersection.
 */
static GLfloat
clip_intersect_y(GLfloat p1x, GLfloat p1y, GLfloat p2x, GLfloat p2y,
		 GLfloat x_arg)
{
	GLfloat a;
	GLfloat diff = float_difference(p1x, p2x);

	/* Practically vertical line segment, yet the end points have already
	 * been determined to be on different sides of the line. Therefore
	 * the line segment is part of the line and intersects everywhere.
	 * Return the end point, so we use the whole line segment.
	 */
	if (diff == 0.0f)
		return p2y;

	a = (x_arg - p2x) / diff;
	return p2y + (p1y - p2y) * a;
}

/* A line segment (p1x, p1y)-(p2x, p2y) intersects the line y = y_arg.
 * Compute the x coordinate of the intersection.
 */
static GLfloat
clip_intersect_x(GLfloat p1x, GLfloat p1y, GLfloat p2x, GLfloat p2y,
		 GLfloat y_arg)
{
	GLfloat a;
	GLfloat diff = float_difference(p1y, p2y);

	/* Practically horizontal line segment, yet the end points have already
	 * been determined to be on different sides of the line. Therefore
	 * the line segment is part of the line and intersects everywhere.
	 * Return the end point, so we use the whole line segment.
	 */
	if (diff == 0.0f)
		return p2x;

	a = (y_arg - p2y) / diff;
	return p2x + (p1x - p2x) * a;
}

enum path_transition {
	PATH_TRANSITION_OUT_TO_OUT = 0,
	PATH_TRANSITION_OUT_TO_IN = 1,
	PATH_TRANSITION_IN_TO_OUT = 2,
	PATH_TRANSITION_IN_TO_IN = 3,
};

static void
clip_append_vertex(struct clip_context *ctx, GLfloat x, GLfloat y)
{
	*ctx->vertices.x++ = x;
	*ctx->vertices.y++ = y;
}

static enum path_transition
path_transition_left_edge(struct clip_context *ctx, GLfloat x, GLfloat y)
{
	return ((ctx->prev.x >= ctx->clip.x1) << 1) | (x >= ctx->clip.x1);
}

static enum path_transition
path_transition_right_edge(struct clip_context *ctx, GLfloat x, GLfloat y)
{
	return ((ctx->prev.x < ctx->clip.x2) << 1) | (x < ctx->clip.x2);
}

static enum path_transition
path_transition_top_edge(struct clip_context *ctx, GLfloat x, GLfloat y)
{
	return ((ctx->prev.y >= ctx->clip.y1) << 1) | (y >= ctx->clip.y1);
}

static enum path_transition
path_transition_bottom_edge(struct clip_context *ctx, GLfloat x, GLfloat y)
{
	return ((ctx->prev.y < ctx->clip.y2) << 1) | (y < ctx->clip.y2);
}

static void
clip_polygon_leftright(struct clip_context *ctx,
		       enum path_transition transition,
		       GLfloat x, GLfloat y, GLfloat clip_x)
{
	GLfloat yi;

	switch (transition) {
	case PATH_TRANSITION_IN_TO_IN:
		clip_append_vertex(ctx, x, y);
		break;
	case PATH_TRANSITION_IN_TO_OUT:
		yi = clip_intersect_y(ctx->prev.x, ctx->prev.y, x, y, clip_x);
		clip_append_vertex(ctx, clip_x, yi);
		break;
	case PATH_TRANSITION_OUT_TO_IN:
		yi = clip_intersect_y(ctx->prev.x, ctx->prev.y, x, y, clip_x);
		clip_append_vertex(ctx, clip_x, yi);
		clip_append_vertex(ctx, x, y);
		break;
	case PATH_TRANSITION_OUT_TO_OUT:
		/* nothing */
		break;
	default:
		assert(0 && "bad enum path_transition");
	}

	ctx->prev.x = x;
	ctx->prev.y = y;
}

static void
clip_polygon_topbottom(struct clip_context *ctx,
		       enum path_transition transition,
		       GLfloat x, GLfloat y, GLfloat clip_y)
{
	GLfloat xi;

	switch (transition) {
	case PATH_TRANSITION_IN_TO_IN:
		clip_append_vertex(ctx, x, y);
		break;
	case PATH_TRANSITION_IN_TO_OUT:
		xi = clip_intersect_x(ctx->prev.x, ctx->prev.y, x, y, clip_y);
		clip_append_vertex(ctx, xi, clip_y);
		break;
	case PATH_TRANSITION_OUT_TO_IN:
		xi = clip_intersect_x(ctx->prev.x, ctx->prev.y, x, y, clip_y);
		clip_append_vertex(ctx, xi, clip_y);
		clip_append_vertex(ctx, x, y);
		break;
	case PATH_TRANSITION_OUT_TO_OUT:
		/* nothing */
		break;
	default:
		assert(0 && "bad enum path_transition");
	}

	ctx->prev.x = x;
	ctx->prev.y = y;
}

static void
clip_context_prepare(struct clip_context *ctx, const struct polygon8 *src,
		      GLfloat *dst_x, GLfloat *dst_y)
{
	ctx->prev.x = src->x[src->n - 1];
	ctx->prev.y = src->y[src->n - 1];
	ctx->vertices.x = dst_x;
	ctx->vertices.y = dst_y;
}

static int
clip_polygon_left(struct clip_context *ctx, const struct polygon8 *src,
		  GLfloat *dst_x, GLfloat *dst_y)
{
	enum path_transition trans;
	int i;

	clip_context_prepare(ctx, src, dst_x, dst_y);
	for (i = 0; i < src->n; i++) {
		trans = path_transition_left_edge(ctx, src->x[i], src->y[i]);
		clip_polygon_leftright(ctx, trans, src->x[i], src->y[i],
				       ctx->clip.x1);
	}
	return ctx->vertices.x - dst_x;
}

static int
clip_polygon_right(struct clip_context *ctx, const struct polygon8 *src,
		   GLfloat *dst_x, GLfloat *dst_y)
{
	enum path_transition trans;
	int i;

	clip_context_prepare(ctx, src, dst_x, dst_y);
	for (i = 0; i < src->n; i++) {
		trans = path_transition_right_edge(ctx, src->x[i], src->y[i]);
		clip_polygon_leftright(ctx, trans, src->x[i], src->y[i],
				       ctx->clip.x2);
	}
	return ctx->vertices.x - dst_x;
}

static int
clip_polygon_top(struct clip_context *ctx, const struct polygon8 *src,
		 GLfloat *dst_x, GLfloat *dst_y)
{
	enum path_transition trans;
	int i;

	clip_context_prepare(ctx, src, dst_x, dst_y);
	for (i = 0; i < src->n; i++) {
		trans = path_transition_top_edge(ctx, src->x[i], src->y[i]);
		clip_polygon_topbottom(ctx, trans, src->x[i], src->y[i],
				       ctx->clip.y1);
	}
	return ctx->vertices.x - dst_x;
}

static int
clip_polygon_bottom(struct clip_context *ctx, const struct polygon8 *src,
		    GLfloat *dst_x, GLfloat *dst_y)
{
	enum path_transition trans;
	int i;

	clip_context_prepare(ctx, src, dst_x, dst_y);
	for (i = 0; i < src->n; i++) {
		trans = path_transition_bottom_edge(ctx, src->x[i], src->y[i]);
		clip_polygon_topbottom(ctx, trans, src->x[i], src->y[i],
				       ctx->clip.y2);
	}
	return ctx->vertices.x - dst_x;
}

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define clip(x, a, b)  min(max(x, a), b)

/*
 * Compute the boundary vertices of the intersection of the global coordinate
 * aligned rectangle 'rect', and an arbitrary quadrilateral produced from
 * 'surf_rect' when transformed from surface coordinates into global coordinates.
 * The vertices are written to 'ex' and 'ey', and the return value is the
 * number of vertices. Vertices are produced in clockwise winding order.
 * Guarantees to produce either zero vertices, or 3-8 vertices with non-zero
 * polygon area.
 */
static int
calculate_edges(struct weston_surface *es, pixman_box32_t *rect,
		pixman_box32_t *surf_rect, GLfloat *ex, GLfloat *ey)
{
	struct polygon8 polygon;
	struct clip_context ctx;
	int i, n;
	GLfloat min_x, max_x, min_y, max_y;
	struct polygon8 surf = {
		{ surf_rect->x1, surf_rect->x2, surf_rect->x2, surf_rect->x1 },
		{ surf_rect->y1, surf_rect->y1, surf_rect->y2, surf_rect->y2 },
		4
	};

	ctx.clip.x1 = rect->x1;
	ctx.clip.y1 = rect->y1;
	ctx.clip.x2 = rect->x2;
	ctx.clip.y2 = rect->y2;

	/* transform surface to screen space: */
	for (i = 0; i < surf.n; i++)
		weston_surface_to_global_float(es, surf.x[i], surf.y[i],
					       &surf.x[i], &surf.y[i]);

	/* find bounding box: */
	min_x = max_x = surf.x[0];
	min_y = max_y = surf.y[0];

	for (i = 1; i < surf.n; i++) {
		min_x = min(min_x, surf.x[i]);
		max_x = max(max_x, surf.x[i]);
		min_y = min(min_y, surf.y[i]);
		max_y = max(max_y, surf.y[i]);
	}

	/* First, simple bounding box check to discard early transformed
	 * surface rects that do not intersect with the clip region:
	 */
	if ((min_x >= ctx.clip.x2) || (max_x <= ctx.clip.x1) ||
	    (min_y >= ctx.clip.y2) || (max_y <= ctx.clip.y1))
		return 0;

	/* Simple case, bounding box edges are parallel to surface edges,
	 * there will be only four edges.  We just need to clip the surface
	 * vertices to the clip rect bounds:
	 */
	if (!es->transform.enabled) {
		for (i = 0; i < surf.n; i++) {
			ex[i] = clip(surf.x[i], ctx.clip.x1, ctx.clip.x2);
			ey[i] = clip(surf.y[i], ctx.clip.y1, ctx.clip.y2);
		}
		return surf.n;
	}

	/* Transformed case: use a general polygon clipping algorithm to
	 * clip the surface rectangle with each side of 'rect'.
	 * The algorithm is Sutherland-Hodgman, as explained in
	 * http://www.codeguru.com/cpp/misc/misc/graphics/article.php/c8965/Polygon-Clipping.htm
	 * but without looking at any of that code.
	 */
	polygon.n = clip_polygon_left(&ctx, &surf, polygon.x, polygon.y);
	surf.n = clip_polygon_right(&ctx, &polygon, surf.x, surf.y);
	polygon.n = clip_polygon_top(&ctx, &surf, polygon.x, polygon.y);
	surf.n = clip_polygon_bottom(&ctx, &polygon, surf.x, surf.y);

	/* Get rid of duplicate vertices */
	ex[0] = surf.x[0];
	ey[0] = surf.y[0];
	n = 1;
	for (i = 1; i < surf.n; i++) {
		if (float_difference(ex[n - 1], surf.x[i]) == 0.0f &&
		    float_difference(ey[n - 1], surf.y[i]) == 0.0f)
			continue;
		ex[n] = surf.x[i];
		ey[n] = surf.y[i];
		n++;
	}
	if (float_difference(ex[n - 1], surf.x[0]) == 0.0f &&
	    float_difference(ey[n - 1], surf.y[0]) == 0.0f)
		n--;

	if (n < 3)
		return 0;

	return n;
}


/* ---------------------- copied ends -----------------------*/

static void
geometry_set_phi(struct geometry *g, float phi)
{
	g->phi = phi;
	g->s = sin(phi);
	g->c = cos(phi);
}

static void
geometry_init(struct geometry *g)
{
	g->clip.x1 = -50;
	g->clip.y1 = -50;
	g->clip.x2 = -10;
	g->clip.y2 = -10;

	g->surf.x1 = -20;
	g->surf.y1 = -20;
	g->surf.x2 = 20;
	g->surf.y2 = 20;

	geometry_set_phi(g, 0.0);
}

struct ui_state {
	uint32_t button;
	int down;

	int down_pos[2];
	struct geometry geometry;
};

struct cliptest {
	struct window *window;
	struct widget *widget;
	struct display *display;
	int fullscreen;

	struct ui_state ui;

	struct geometry geometry;
	struct weston_surface surface;
};

static void
draw_polygon_closed(cairo_t *cr, GLfloat *x, GLfloat *y, int n)
{
	int i;

	cairo_move_to(cr, x[0], y[0]);
	for (i = 1; i < n; i++)
		cairo_line_to(cr, x[i], y[i]);
	cairo_line_to(cr, x[0], y[0]);
}

static void
draw_polygon_labels(cairo_t *cr, GLfloat *x, GLfloat *y, int n)
{
	char str[16];
	int i;

	for (i = 0; i < n; i++) {
		snprintf(str, 16, "%d", i);
		cairo_move_to(cr, x[i], y[i]);
		cairo_show_text(cr, str);
	}
}

static void
draw_coordinates(cairo_t *cr, double ox, double oy, GLfloat *x, GLfloat *y, int n)
{
	char str[64];
	int i;
	cairo_font_extents_t ext;

	cairo_font_extents(cr, &ext);
	for (i = 0; i < n; i++) {
		snprintf(str, 64, "%d: %14.9f, %14.9f", i, x[i], y[i]);
		cairo_move_to(cr, ox, oy + ext.height * (i + 1));
		cairo_show_text(cr, str);
	}
}

static void
draw_box(cairo_t *cr, pixman_box32_t *box, struct weston_surface *surface)
{
	GLfloat x[4], y[4];

	if (surface) {
		weston_surface_to_global_float(surface, box->x1, box->y1, &x[0], &y[0]);
		weston_surface_to_global_float(surface, box->x2, box->y1, &x[1], &y[1]);
		weston_surface_to_global_float(surface, box->x2, box->y2, &x[2], &y[2]);
		weston_surface_to_global_float(surface, box->x1, box->y2, &x[3], &y[3]);
	} else {
		x[0] = box->x1; y[0] = box->y1;
		x[1] = box->x2; y[1] = box->y1;
		x[2] = box->x2; y[2] = box->y2;
		x[3] = box->x1; y[3] = box->y2;
	}

	draw_polygon_closed(cr, x, y, 4);
}

static void
draw_geometry(cairo_t *cr, struct weston_surface *surface,
	      GLfloat *ex, GLfloat *ey, int n)
{
	struct geometry *g = surface->geometry;
	GLfloat cx, cy;

	draw_box(cr, &g->surf, surface);
	cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.4);
	cairo_fill(cr);
	weston_surface_to_global_float(surface, g->surf.x1 - 4, g->surf.y1 - 4, &cx, &cy);
	cairo_arc(cr, cx, cy, 1.5, 0.0, 2.0 * M_PI);
	if (surface->transform.enabled == 0)
		cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.8);
	cairo_fill(cr);

	draw_box(cr, &g->clip, NULL);
	cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.4);
	cairo_fill(cr);

	draw_polygon_closed(cr, ex, ey, n);
	cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
	cairo_stroke(cr);

	cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.5);
	draw_polygon_labels(cr, ex, ey, n);
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct cliptest *cliptest = data;
	struct geometry *g = cliptest->surface.geometry;
	struct rectangle allocation;
	cairo_t *cr;
	cairo_surface_t *surface;
	GLfloat ex[8];
	GLfloat ey[8];
	int n;

	n = calculate_edges(&cliptest->surface, &g->clip, &g->surf, ex, ey);

	widget_get_allocation(cliptest->widget, &allocation);

	surface = window_get_surface(cliptest->window);
	cr = cairo_create(surface);
	widget_get_allocation(cliptest->widget, &allocation);
	cairo_rectangle(cr, allocation.x, allocation.y,
			allocation.width, allocation.height);
	cairo_clip(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_paint(cr);

	cairo_translate(cr, allocation.x, allocation.y);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, allocation.width / 2.0, 0.0);
	cairo_line_to(cr, allocation.width / 2.0, allocation.height);
	cairo_move_to(cr, 0.0, allocation.height / 2.0);
	cairo_line_to(cr, allocation.width, allocation.height / 2.0);
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1.0);
	cairo_stroke(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_push_group(cr);
		cairo_translate(cr, allocation.width / 2.0,
				allocation.height / 2.0);
		cairo_scale(cr, 4.0, 4.0);
		cairo_set_line_width(cr, 0.5);
		cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
		cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
				       CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 5.0);
		draw_geometry(cr, &cliptest->surface, ex, ey, n);
	cairo_pop_group_to_source(cr);
	cairo_paint(cr);

	cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);
	cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);
	draw_coordinates(cr, 10.0, 10.0, ex, ey, n);

	cairo_destroy(cr);

	cairo_surface_destroy(surface);
}

static int
motion_handler(struct widget *widget, struct input *input,
	       uint32_t time, float x, float y, void *data)
{
	struct cliptest *cliptest = data;
	struct ui_state *ui = &cliptest->ui;
	struct geometry *ref = &ui->geometry;
	struct geometry *geom = &cliptest->geometry;
	float dx, dy;

	if (!ui->down)
		return CURSOR_LEFT_PTR;

	dx = (x - ui->down_pos[0]) * 0.25;
	dy = (y - ui->down_pos[1]) * 0.25;

	switch (ui->button) {
	case BTN_LEFT:
		geom->clip.x1 = ref->clip.x1 + dx;
		geom->clip.y1 = ref->clip.y1 + dy;
		/* fall through */
	case BTN_RIGHT:
		geom->clip.x2 = ref->clip.x2 + dx;
		geom->clip.y2 = ref->clip.y2 + dy;
		break;
	default:
		return CURSOR_LEFT_PTR;
	}

	widget_schedule_redraw(cliptest->widget);
	return CURSOR_BLANK;
}

static void
button_handler(struct widget *widget, struct input *input,
	       uint32_t time, uint32_t button,
	       enum wl_pointer_button_state state, void *data)
{
	struct cliptest *cliptest = data;
	struct ui_state *ui = &cliptest->ui;

	ui->button = button;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		ui->down = 1;
		input_get_position(input, &ui->down_pos[0], &ui->down_pos[1]);
	} else {
		ui->down = 0;
		ui->geometry = cliptest->geometry;
	}
}

static void
axis_handler(struct widget *widget, struct input *input, uint32_t time,
	     uint32_t axis, wl_fixed_t value, void *data)
{
	struct cliptest *cliptest = data;
	struct geometry *geom = &cliptest->geometry;

	if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
		return;

	geometry_set_phi(geom, geom->phi +
				(M_PI / 12.0) * wl_fixed_to_double(value));
	cliptest->surface.transform.enabled = 1;

	widget_schedule_redraw(cliptest->widget);
}

static void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym,
	    enum wl_keyboard_key_state state, void *data)
{
	struct cliptest *cliptest = data;
	struct geometry *g = &cliptest->geometry;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	switch (sym) {
	case XKB_KEY_Escape:
		display_exit(cliptest->display);
		return;
	case XKB_KEY_w:
		g->clip.y1 -= 1;
		g->clip.y2 -= 1;
		break;
	case XKB_KEY_a:
		g->clip.x1 -= 1;
		g->clip.x2 -= 1;
		break;
	case XKB_KEY_s:
		g->clip.y1 += 1;
		g->clip.y2 += 1;
		break;
	case XKB_KEY_d:
		g->clip.x1 += 1;
		g->clip.x2 += 1;
		break;
	case XKB_KEY_i:
		g->clip.y2 -= 1;
		break;
	case XKB_KEY_j:
		g->clip.x2 -= 1;
		break;
	case XKB_KEY_k:
		g->clip.y2 += 1;
		break;
	case XKB_KEY_l:
		g->clip.x2 += 1;
		break;
	case XKB_KEY_n:
		geometry_set_phi(g, g->phi + (M_PI / 24.0));
		cliptest->surface.transform.enabled = 1;
		break;
	case XKB_KEY_m:
		geometry_set_phi(g, g->phi - (M_PI / 24.0));
		cliptest->surface.transform.enabled = 1;
		break;
	case XKB_KEY_r:
		geometry_set_phi(g, 0.0);
		cliptest->surface.transform.enabled = 0;
		break;
	default:
		return;
	}

	widget_schedule_redraw(cliptest->widget);
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct cliptest *cliptest = data;

	window_schedule_redraw(cliptest->window);
}

static void
fullscreen_handler(struct window *window, void *data)
{
	struct cliptest *cliptest = data;

	cliptest->fullscreen ^= 1;
	window_set_fullscreen(window, cliptest->fullscreen);
}

static struct cliptest *
cliptest_create(struct display *display)
{
	struct cliptest *cliptest;

	cliptest = xzalloc(sizeof *cliptest);
	cliptest->surface.geometry = &cliptest->geometry;
	cliptest->surface.transform.enabled = 0;
	geometry_init(&cliptest->geometry);
	geometry_init(&cliptest->ui.geometry);

	cliptest->window = window_create(display);
	cliptest->widget = window_frame_create(cliptest->window, cliptest);
	window_set_title(cliptest->window, "cliptest");
	cliptest->display = display;

	window_set_user_data(cliptest->window, cliptest);
	widget_set_redraw_handler(cliptest->widget, redraw_handler);
	widget_set_button_handler(cliptest->widget, button_handler);
	widget_set_motion_handler(cliptest->widget, motion_handler);
	widget_set_axis_handler(cliptest->widget, axis_handler);

	window_set_keyboard_focus_handler(cliptest->window,
					  keyboard_focus_handler);
	window_set_key_handler(cliptest->window, key_handler);
	window_set_fullscreen_handler(cliptest->window, fullscreen_handler);

	/* set minimum size */
	widget_schedule_resize(cliptest->widget, 200, 100);

	/* set current size */
	widget_schedule_resize(cliptest->widget, 500, 400);

	return cliptest;
}

static struct timespec begin_time;

static void
reset_timer(void)
{
	clock_gettime(CLOCK_MONOTONIC, &begin_time);
}

static double
read_timer(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)(t.tv_sec - begin_time.tv_sec) +
	       1e-9 * (t.tv_nsec - begin_time.tv_nsec);
}

static int
benchmark(void)
{
	struct weston_surface surface;
	struct geometry geom;
	GLfloat ex[8], ey[8];
	int i;
	double t;
	const int N = 1000000;

	geom.clip.x1 = -19;
	geom.clip.y1 = -19;
	geom.clip.x2 = 19;
	geom.clip.y2 = 19;

	geom.surf.x1 = -20;
	geom.surf.y1 = -20;
	geom.surf.x2 = 20;
	geom.surf.y2 = 20;

	geometry_set_phi(&geom, 0.0);

	surface.transform.enabled = 1;
	surface.geometry = &geom;

	reset_timer();
	for (i = 0; i < N; i++) {
		geometry_set_phi(&geom, (float)i / 360.0f);
		calculate_edges(&surface, &geom.clip, &geom.surf, ex, ey);
	}
	t = read_timer();

	printf("%d calls took %g s, average %g us/call\n", N, t, t / N * 1e6);

	return 0;
}

int
main(int argc, char *argv[])
{
	struct display *d;
	struct cliptest *cliptest;

	if (argc > 1)
		return benchmark();

	d = display_create(&argc, argv);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	cliptest = cliptest_create(d);
	display_run(d);

	widget_destroy(cliptest->widget);
	window_destroy(cliptest->window);
	free(cliptest);

	return 0;
}

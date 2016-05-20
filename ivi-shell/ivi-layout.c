/*
 * Copyright (C) 2013 DENSO CORPORATION
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * Implementation of ivi-layout library. The actual view on ivi_screen is
 * not updated until ivi_layout_commit_changes is called. An overview from
 * calling API for updating properties of ivi_surface/ivi_layer to asking
 * compositor to compose them by using weston_compositor_schedule_repaint,
 * 0/ initialize this library by ivi_layout_init_with_compositor
 *    with (struct weston_compositor *ec) from ivi-shell.
 * 1/ When an API for updating properties of ivi_surface/ivi_layer, it updates
 *    pending prop of ivi_surface/ivi_layer/ivi_screen which are structure to
 *    store properties.
 * 2/ Before calling commitChanges, in case of calling an API to get a property,
 *    return current property, not pending property.
 * 3/ At the timing of calling ivi_layout_commitChanges, pending properties
 *    are applied to properties.
 *
 *    *) ivi_layout_commitChanges is also called by transition animation
 *    per each frame. See ivi-layout-transition.c in details. Transition
 *    animation interpolates frames between previous properties of ivi_surface
 *    and new ones.
 *    For example, when a property of ivi_surface is changed from invisibile
 *    to visibile, it behaves like fade-in. When ivi_layout_commitChange is
 *    called during transition animation, it cancels the transition and
 *    re-start transition to new properties from current properties of final
 *    frame just before the cancellation.
 *
 * 4/ According properties, set transformation by using weston_matrix and
 *    weston_view per ivi_surfaces and ivi_layers in while loop.
 * 5/ Set damage and trigger transform by using weston_view_geometry_dirty.
 * 6/ Notify update of properties.
 * 7/ Trigger composition by weston_compositor_schedule_repaint.
 *
 */
#include "config.h"

#include <string.h>
#include <assert.h>

#include "compositor.h"
#include "ivi-shell.h"
#include "ivi-layout-export.h"
#include "ivi-layout-private.h"
#include "ivi-layout-shell.h"

#include "shared/helpers.h"
#include "shared/os-compatibility.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

struct ivi_layout;

struct ivi_layout_screen {
	struct wl_list link;

	struct ivi_layout *layout;
	struct weston_output *output;

	struct {
		struct wl_list layer_list;
		struct wl_list link;
	} pending;

	struct {
		int dirty;
		struct wl_list layer_list;
		struct wl_list link;
	} order;
};

struct ivi_rectangle
{
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

static struct ivi_layout ivilayout = {0};

struct ivi_layout *
get_instance(void)
{
	return &ivilayout;
}

/**
 * Internal API to add/remove an ivi_layer to/from ivi_screen.
 */
static struct ivi_layout_surface *
get_surface(struct wl_list *surf_list, uint32_t id_surface)
{
	struct ivi_layout_surface *ivisurf;

	wl_list_for_each(ivisurf, surf_list, link) {
		if (ivisurf->id_surface == id_surface) {
			return ivisurf;
		}
	}

	return NULL;
}

static struct ivi_layout_layer *
get_layer(struct wl_list *layer_list, uint32_t id_layer)
{
	struct ivi_layout_layer *ivilayer;

	wl_list_for_each(ivilayer, layer_list, link) {
		if (ivilayer->id_layer == id_layer) {
			return ivilayer;
		}
	}

	return NULL;
}

static struct weston_view *
get_weston_view(struct ivi_layout_surface *ivisurf)
{
	struct weston_view *view = NULL;

	assert(ivisurf->surface != NULL);

	/* One view per surface */
	if(wl_list_empty(&ivisurf->surface->views))
		view = NULL;
	else
		view = wl_container_of(ivisurf->surface->views.next, view, surface_link);

	return view;
}

static struct ivi_layout_screen *
get_screen_from_output(struct weston_output *output)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_screen *iviscrn = NULL;

	wl_list_for_each(iviscrn, &layout->screen_list, link) {
		if (iviscrn->output == output)
			return iviscrn;
	}

	return NULL;
}

/**
 * Called at destruction of wl_surface/ivi_surface
 */
void
ivi_layout_surface_destroy(struct ivi_layout_surface *ivisurf)
{
	struct ivi_layout *layout = get_instance();

	if (ivisurf == NULL) {
		weston_log("%s: invalid argument\n", __func__);
		return;
	}

	wl_list_remove(&ivisurf->transform.link);
	wl_list_remove(&ivisurf->pending.link);
	wl_list_remove(&ivisurf->order.link);
	wl_list_remove(&ivisurf->link);

	wl_signal_emit(&layout->surface_notification.removed, ivisurf);

	ivi_layout_remove_all_surface_transitions(ivisurf);

	free(ivisurf);
}

/**
 * Internal API to initialize ivi_screens found from output_list of weston_compositor.
 * Called by ivi_layout_init_with_compositor.
 */
static void
create_screen(struct weston_compositor *ec)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_screen *iviscrn = NULL;
	struct weston_output *output = NULL;

	wl_list_for_each(output, &ec->output_list, link) {
		iviscrn = calloc(1, sizeof *iviscrn);
		if (iviscrn == NULL) {
			weston_log("fails to allocate memory\n");
			continue;
		}

		iviscrn->layout = layout;

		iviscrn->output = output;

		wl_list_init(&iviscrn->pending.layer_list);
		wl_list_init(&iviscrn->pending.link);

		wl_list_init(&iviscrn->order.layer_list);
		wl_list_init(&iviscrn->order.link);

		wl_list_insert(&layout->screen_list, &iviscrn->link);
	}
}

/**
 * Internal APIs to initialize properties of ivi_surface/ivi_layer when they are created.
 */
static void
init_layer_properties(struct ivi_layout_layer_properties *prop,
		      int32_t width, int32_t height)
{
	memset(prop, 0, sizeof *prop);
	prop->opacity = wl_fixed_from_double(1.0);
	prop->source_width = width;
	prop->source_height = height;
	prop->dest_width = width;
	prop->dest_height = height;
}

static void
init_surface_properties(struct ivi_layout_surface_properties *prop)
{
	memset(prop, 0, sizeof *prop);
	prop->opacity = wl_fixed_from_double(1.0);
	/*
	 * FIXME: this shall be fixed by ivi-layout-transition.
	 */
	prop->dest_width = 1;
	prop->dest_height = 1;
}

/**
 * Internal APIs to be called from ivi_layout_commit_changes.
 */
static void
update_opacity(struct ivi_layout_layer *ivilayer,
	       struct ivi_layout_surface *ivisurf)
{
	struct weston_view *tmpview = NULL;
	double layer_alpha = wl_fixed_to_double(ivilayer->prop.opacity);
	double surf_alpha  = wl_fixed_to_double(ivisurf->prop.opacity);

	tmpview = get_weston_view(ivisurf);
	assert(tmpview != NULL);
	tmpview->alpha = layer_alpha * surf_alpha;
}

static void
get_rotate_values(enum wl_output_transform orientation,
		  float *v_sin,
		  float *v_cos)
{
	switch (orientation) {
	case WL_OUTPUT_TRANSFORM_90:
		*v_sin = 1.0f;
		*v_cos = 0.0f;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		*v_sin = 0.0f;
		*v_cos = -1.0f;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		*v_sin = -1.0f;
		*v_cos = 0.0f;
		break;
	case WL_OUTPUT_TRANSFORM_NORMAL:
	default:
		*v_sin = 0.0f;
		*v_cos = 1.0f;
		break;
	}
}

static void
get_scale(enum wl_output_transform orientation,
	  float dest_width,
	  float dest_height,
	  float source_width,
	  float source_height,
	  float *scale_x,
	  float *scale_y)
{
	switch (orientation) {
	case WL_OUTPUT_TRANSFORM_90:
		*scale_x = dest_width / source_height;
		*scale_y = dest_height / source_width;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		*scale_x = dest_width / source_width;
		*scale_y = dest_height / source_height;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		*scale_x = dest_width / source_height;
		*scale_y = dest_height / source_width;
		break;
	case WL_OUTPUT_TRANSFORM_NORMAL:
	default:
		*scale_x = dest_width / source_width;
		*scale_y = dest_height / source_height;
		break;
	}
}

static void
calc_transformation_matrix(struct ivi_rectangle *source_rect,
			   struct ivi_rectangle *dest_rect,
			   enum wl_output_transform orientation,
			   struct weston_matrix *m)
{
	float source_center_x;
	float source_center_y;
	float vsin;
	float vcos;
	float scale_x;
	float scale_y;
	float translate_x;
	float translate_y;

	source_center_x = source_rect->x + source_rect->width * 0.5f;
	source_center_y = source_rect->y + source_rect->height * 0.5f;
	weston_matrix_translate(m, -source_center_x, -source_center_y, 0.0f);

	get_rotate_values(orientation, &vsin, &vcos);
	weston_matrix_rotate_xy(m, vcos, vsin);

	get_scale(orientation,
		  dest_rect->width,
		  dest_rect->height,
		  source_rect->width,
		  source_rect->height,
		  &scale_x,
		  &scale_y);
	weston_matrix_scale(m, scale_x, scale_y, 1.0f);

	translate_x = dest_rect->width * 0.5f + dest_rect->x;
	translate_y = dest_rect->height * 0.5f + dest_rect->y;
	weston_matrix_translate(m, translate_x, translate_y, 0.0f);
}

/*
 * This computes intersected rect_output from two ivi_rectangles
 */
static void
ivi_rectangle_intersect(const struct ivi_rectangle *rect1,
		        const struct ivi_rectangle *rect2,
		        struct ivi_rectangle *rect_output)
{
	int32_t rect1_right = rect1->x + rect1->width;
	int32_t rect1_bottom = rect1->y + rect1->height;
	int32_t rect2_right = rect2->x + rect2->width;
	int32_t rect2_bottom = rect2->y + rect2->height;

	rect_output->x = max(rect1->x, rect2->x);
	rect_output->y = max(rect1->y, rect2->y);
	rect_output->width = rect1_right < rect2_right ?
			     rect1_right - rect_output->x :
			     rect2_right - rect_output->x;
	rect_output->height = rect1_bottom < rect2_bottom ?
			      rect1_bottom - rect_output->y :
			      rect2_bottom - rect_output->y;

	if (rect_output->width < 0 || rect_output->height < 0) {
		rect_output->width = 0;
		rect_output->height = 0;
	}
}

/*
 * Transform rect_input by the inverse of matrix, intersect with boundingbox,
 * and store the result in rect_output.
 * The boundingbox must be given in the same coordinate space as rect_output.
 * Additionally, there are the following restrictions on the matrix:
 * - no projective transformations
 * - no skew
 * - only multiples of 90-degree rotations supported
 *
 * In failure case of weston_matrix_invert, rect_output is set to boundingbox
 * as a fail-safe with log.
 */
static void
calc_inverse_matrix_transform(const struct weston_matrix *matrix,
			      const struct ivi_rectangle *rect_input,
			      const struct ivi_rectangle *boundingbox,
			      struct ivi_rectangle *rect_output)
{
	struct weston_matrix m;
	struct weston_vector top_left;
	struct weston_vector bottom_right;

	assert(boundingbox != rect_output);

	if (weston_matrix_invert(&m, matrix) < 0) {
		weston_log("ivi-shell: calc_inverse_matrix_transform fails to invert a matrix.\n");
		weston_log("ivi-shell: boundingbox is set to the rect_output.\n");
		rect_output->x = boundingbox->x;
		rect_output->y = boundingbox->y;
		rect_output->width = boundingbox->width;
		rect_output->height = boundingbox->height;
	}

	/* The vectors and matrices involved will always produce f[3] == 1.0. */
	top_left.f[0] = rect_input->x;
	top_left.f[1] = rect_input->y;
	top_left.f[2] = 0.0f;
	top_left.f[3] = 1.0f;

	bottom_right.f[0] = rect_input->x + rect_input->width;
	bottom_right.f[1] = rect_input->y + rect_input->height;
	bottom_right.f[2] = 0.0f;
	bottom_right.f[3] = 1.0f;

	weston_matrix_transform(&m, &top_left);
	weston_matrix_transform(&m, &bottom_right);

	if (top_left.f[0] < bottom_right.f[0]) {
		rect_output->x = top_left.f[0];
		rect_output->width = bottom_right.f[0] - rect_output->x;
	} else {
		rect_output->x = bottom_right.f[0];
		rect_output->width = top_left.f[0] - rect_output->x;
	}

	if (top_left.f[1] < bottom_right.f[1]) {
		rect_output->y = top_left.f[1];
		rect_output->height = bottom_right.f[1] - rect_output->y;
	} else {
		rect_output->y = bottom_right.f[1];
		rect_output->height = top_left.f[1] - rect_output->y;
	}

	ivi_rectangle_intersect(rect_output, boundingbox, rect_output);
}

/**
 * This computes the whole transformation matrix:m from surface-local
 * coordinates to multi-screen coordinates, which are global coordinates.
 * It is assumed that weston_view::geometry.{x,y} are zero.
 *
 * Additionally, this computes the mask on surface-local coordinates as an
 * ivi_rectangle. This can be set to weston_view_set_mask.
 *
 * The mask is computed by following steps
 * - destination rectangle of layer is transformed to multi-screen coordinates,
 *   global coordinates. This is done by adding weston_output.{x,y} in simple
 *   because there is no scaled and rotated transformation.
 * - destination rectangle of layer in multi-screen coordinates needs to be
 *   intersected inside of a screen the layer is assigned to. This is because
 *   overlapped region of weston surface in another screen shall not be
 *   displayed according to ivi use case.
 * - destination rectangle of layer
 *   - in multi-screen coordinates,
 *   - and intersected inside of an assigned screen,
 *   is inversed to surface-local coordinates by inversed matrix:m.
 * - the area is intersected by intersected area between weston_surface and
 *   source rectangle of ivi_surface.
 */
static void
calc_surface_to_global_matrix_and_mask_to_weston_surface(
	struct ivi_layout_screen  *iviscrn,
	struct ivi_layout_layer *ivilayer,
	struct ivi_layout_surface *ivisurf,
	struct weston_matrix *m,
	struct ivi_rectangle *result)
{
	const struct ivi_layout_surface_properties *sp = &ivisurf->prop;
	const struct ivi_layout_layer_properties *lp = &ivilayer->prop;
	struct weston_output *output = iviscrn->output;
	struct ivi_rectangle weston_surface_rect = { 0,
						     0,
						     ivisurf->surface->width,
						     ivisurf->surface->height };
	struct ivi_rectangle surface_source_rect = { sp->source_x,
						     sp->source_y,
						     sp->source_width,
						     sp->source_height };
	struct ivi_rectangle surface_dest_rect =   { sp->dest_x,
						     sp->dest_y,
						     sp->dest_width,
						     sp->dest_height };
	struct ivi_rectangle layer_source_rect =   { lp->source_x,
						     lp->source_y,
						     lp->source_width,
						     lp->source_height };
	struct ivi_rectangle layer_dest_rect =     { lp->dest_x,
						     lp->dest_y,
						     lp->dest_width,
						     lp->dest_height };
	struct ivi_rectangle screen_dest_rect =    { output->x,
						     output->y,
						     output->width,
						     output->height };
	struct ivi_rectangle layer_dest_rect_in_global =
						   { lp->dest_x + output->x,
						     lp->dest_y + output->y,
						     lp->dest_width,
						     lp->dest_height };
	struct ivi_rectangle surface_result;
	struct ivi_rectangle layer_dest_rect_in_global_intersected;

	/*
	 * the whole transformation matrix:m from surface-local
	 * coordinates to global coordinates, which is computed by
	 * two steps,
	 * - surface-local coordinates to layer-local coordinates
	 * - layer-local coordinates to single screen-local coordinates
	 * - single screen-local coordinates to multi-screen coordinates,
	 *   which are global coordinates.
	 */
	calc_transformation_matrix(&surface_source_rect,
				   &surface_dest_rect,
				   sp->orientation, m);

	calc_transformation_matrix(&layer_source_rect,
				   &layer_dest_rect,
				   lp->orientation, m);

	weston_matrix_translate(m, output->x, output->y, 0.0f);

	/* this intersected ivi_rectangle would be used for masking
	 * weston_surface
	 */
	ivi_rectangle_intersect(&surface_source_rect, &weston_surface_rect,
				&surface_result);

	/*
	 * destination rectangle of layer in multi screens coordinate
	 * is intersected to avoid displaying outside of an assigned screen.
	 */
	ivi_rectangle_intersect(&layer_dest_rect_in_global, &screen_dest_rect,
				&layer_dest_rect_in_global_intersected);

	/* calc masking area of weston_surface from m */
	calc_inverse_matrix_transform(m,
				      &layer_dest_rect_in_global_intersected,
				      &surface_result,
				      result);
}

static void
update_prop(struct ivi_layout_screen  *iviscrn,
	    struct ivi_layout_layer *ivilayer,
	    struct ivi_layout_surface *ivisurf)
{
	struct weston_view *tmpview;
	struct ivi_rectangle r;
	bool can_calc = true;

	/*In case of no prop change, this just returns*/
	if (!ivilayer->prop.event_mask && !ivisurf->prop.event_mask)
		return;

	update_opacity(ivilayer, ivisurf);

	tmpview = get_weston_view(ivisurf);
	assert(tmpview != NULL);

	if (ivisurf->prop.source_width == 0 || ivisurf->prop.source_height == 0) {
		weston_log("ivi-shell: source rectangle is not yet set by ivi_layout_surface_set_source_rectangle\n");
		can_calc = false;
	}

	if (ivisurf->prop.dest_width == 0 || ivisurf->prop.dest_height == 0) {
		weston_log("ivi-shell: destination rectangle is not yet set by ivi_layout_surface_set_destination_rectangle\n");
		can_calc = false;
	}

	if (can_calc) {
		wl_list_remove(&ivisurf->transform.link);
		weston_matrix_init(&ivisurf->transform.matrix);

		calc_surface_to_global_matrix_and_mask_to_weston_surface(
			iviscrn, ivilayer, ivisurf, &ivisurf->transform.matrix, &r);

		weston_view_set_mask(tmpview, r.x, r.y, r.width, r.height);
		wl_list_insert(&tmpview->geometry.transformation_list,
			       &ivisurf->transform.link);

		weston_view_set_transform_parent(tmpview, NULL);
	}

	ivisurf->update_count++;

	weston_view_geometry_dirty(tmpview);

	weston_surface_damage(ivisurf->surface);
}

static void
commit_changes(struct ivi_layout *layout)
{
	struct ivi_layout_screen  *iviscrn  = NULL;
	struct ivi_layout_layer   *ivilayer = NULL;
	struct ivi_layout_surface *ivisurf  = NULL;

	wl_list_for_each(iviscrn, &layout->screen_list, link) {
		wl_list_for_each(ivilayer, &iviscrn->order.layer_list, order.link) {
			/*
			 * If ivilayer is invisible, weston_view of ivisurf doesn't
			 * need to be modified.
			 */
			if (ivilayer->prop.visibility == false)
				continue;

			wl_list_for_each(ivisurf, &ivilayer->order.surface_list, order.link) {
				/*
				 * If ivilayer is invisible, weston_view of ivisurf doesn't
				 * need to be modified.
				 */
				if (ivisurf->prop.visibility == false)
					continue;

				update_prop(iviscrn, ivilayer, ivisurf);
			}
		}
	}
}

static void
commit_surface_list(struct ivi_layout *layout)
{
	struct ivi_layout_surface *ivisurf = NULL;
	int32_t dest_x = 0;
	int32_t dest_y = 0;
	int32_t dest_width = 0;
	int32_t dest_height = 0;
	int32_t configured = 0;

	wl_list_for_each(ivisurf, &layout->surface_list, link) {
		if (ivisurf->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_VIEW_DEFAULT) {
			dest_x = ivisurf->prop.dest_x;
			dest_y = ivisurf->prop.dest_y;
			dest_width = ivisurf->prop.dest_width;
			dest_height = ivisurf->prop.dest_height;

			ivi_layout_transition_move_resize_view(ivisurf,
							       ivisurf->pending.prop.dest_x,
							       ivisurf->pending.prop.dest_y,
							       ivisurf->pending.prop.dest_width,
							       ivisurf->pending.prop.dest_height,
							       ivisurf->pending.prop.transition_duration);

			if (ivisurf->pending.prop.visibility) {
				ivi_layout_transition_visibility_on(ivisurf, ivisurf->pending.prop.transition_duration);
			} else {
				ivi_layout_transition_visibility_off(ivisurf, ivisurf->pending.prop.transition_duration);
			}

			ivisurf->prop = ivisurf->pending.prop;
			ivisurf->prop.dest_x = dest_x;
			ivisurf->prop.dest_y = dest_y;
			ivisurf->prop.dest_width = dest_width;
			ivisurf->prop.dest_height = dest_height;
			ivisurf->prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;
			ivisurf->pending.prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;

		} else if (ivisurf->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_VIEW_DEST_RECT_ONLY) {
			dest_x = ivisurf->prop.dest_x;
			dest_y = ivisurf->prop.dest_y;
			dest_width = ivisurf->prop.dest_width;
			dest_height = ivisurf->prop.dest_height;

			ivi_layout_transition_move_resize_view(ivisurf,
							       ivisurf->pending.prop.dest_x,
							       ivisurf->pending.prop.dest_y,
							       ivisurf->pending.prop.dest_width,
							       ivisurf->pending.prop.dest_height,
							       ivisurf->pending.prop.transition_duration);

			ivisurf->prop = ivisurf->pending.prop;
			ivisurf->prop.dest_x = dest_x;
			ivisurf->prop.dest_y = dest_y;
			ivisurf->prop.dest_width = dest_width;
			ivisurf->prop.dest_height = dest_height;

			ivisurf->prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;
			ivisurf->pending.prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;

		} else if (ivisurf->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_VIEW_FADE_ONLY) {
			configured = 0;
			if (ivisurf->pending.prop.visibility) {
				ivi_layout_transition_visibility_on(ivisurf, ivisurf->pending.prop.transition_duration);
			} else {
				ivi_layout_transition_visibility_off(ivisurf, ivisurf->pending.prop.transition_duration);
			}

			if (ivisurf->prop.dest_width  != ivisurf->pending.prop.dest_width ||
			    ivisurf->prop.dest_height != ivisurf->pending.prop.dest_height) {
				configured = 1;
			}

			ivisurf->prop = ivisurf->pending.prop;
			ivisurf->prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;
			ivisurf->pending.prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;

			if (configured && !is_surface_transition(ivisurf)) {
				shell_surface_send_configure(ivisurf->surface,
							     ivisurf->prop.dest_width,
							     ivisurf->prop.dest_height);
			}
		} else {
			configured = 0;
			if (ivisurf->prop.dest_width  != ivisurf->pending.prop.dest_width ||
			    ivisurf->prop.dest_height != ivisurf->pending.prop.dest_height) {
				configured = 1;
			}

			ivisurf->prop = ivisurf->pending.prop;
			ivisurf->prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;
			ivisurf->pending.prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;

			if (configured && !is_surface_transition(ivisurf)) {
				shell_surface_send_configure(ivisurf->surface,
							     ivisurf->prop.dest_width,
							     ivisurf->prop.dest_height);
			}
		}
	}
}

static void
commit_layer_list(struct ivi_layout *layout)
{
	struct ivi_layout_layer   *ivilayer = NULL;
	struct ivi_layout_surface *ivisurf  = NULL;
	struct ivi_layout_surface *next     = NULL;

	wl_list_for_each(ivilayer, &layout->layer_list, link) {
		if (ivilayer->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_LAYER_MOVE) {
			ivi_layout_transition_move_layer(ivilayer, ivilayer->pending.prop.dest_x, ivilayer->pending.prop.dest_y, ivilayer->pending.prop.transition_duration);
		} else if (ivilayer->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_LAYER_FADE) {
			ivi_layout_transition_fade_layer(ivilayer,ivilayer->pending.prop.is_fade_in,
							 ivilayer->pending.prop.start_alpha,ivilayer->pending.prop.end_alpha,
							 NULL, NULL,
							 ivilayer->pending.prop.transition_duration);
		}
		ivilayer->pending.prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;

		ivilayer->prop = ivilayer->pending.prop;

		if (!ivilayer->order.dirty) {
			continue;
		}

		wl_list_for_each_safe(ivisurf, next, &ivilayer->order.surface_list,
					 order.link) {
			ivisurf->on_layer = NULL;
			wl_list_remove(&ivisurf->order.link);
			wl_list_init(&ivisurf->order.link);
			ivisurf->prop.event_mask |= IVI_NOTIFICATION_REMOVE;
		}

		assert(wl_list_empty(&ivilayer->order.surface_list));

		wl_list_for_each(ivisurf, &ivilayer->pending.surface_list,
					 pending.link) {
			wl_list_remove(&ivisurf->order.link);
			wl_list_insert(&ivilayer->order.surface_list,
				       &ivisurf->order.link);
			ivisurf->on_layer = ivilayer;
			ivisurf->prop.event_mask |= IVI_NOTIFICATION_ADD;
		}

		ivilayer->order.dirty = 0;
	}
}

static void
commit_screen_list(struct ivi_layout *layout)
{
	struct ivi_layout_screen  *iviscrn  = NULL;
	struct ivi_layout_layer   *ivilayer = NULL;
	struct ivi_layout_layer   *next     = NULL;
	struct ivi_layout_surface *ivisurf  = NULL;
	struct weston_view *tmpview = NULL;

	/* Clear view list of layout ivi_layer */
	wl_list_init(&layout->layout_layer.view_list.link);

	wl_list_for_each(iviscrn, &layout->screen_list, link) {
		if (iviscrn->order.dirty) {
			wl_list_for_each_safe(ivilayer, next,
					      &iviscrn->order.layer_list, order.link) {
				ivilayer->on_screen = NULL;
				wl_list_remove(&ivilayer->order.link);
				wl_list_init(&ivilayer->order.link);
				ivilayer->prop.event_mask |= IVI_NOTIFICATION_REMOVE;
			}

			assert(wl_list_empty(&iviscrn->order.layer_list));

			wl_list_for_each(ivilayer, &iviscrn->pending.layer_list,
					 pending.link) {
				/* FIXME: avoid to insert order.link to multiple screens */
				wl_list_remove(&ivilayer->order.link);

				wl_list_insert(&iviscrn->order.layer_list,
					       &ivilayer->order.link);
				ivilayer->on_screen = iviscrn;
				ivilayer->prop.event_mask |= IVI_NOTIFICATION_ADD;
			}

			iviscrn->order.dirty = 0;
		}

		wl_list_for_each(ivilayer, &iviscrn->order.layer_list, order.link) {
			if (ivilayer->prop.visibility == false)
				continue;

			wl_list_for_each(ivisurf, &ivilayer->order.surface_list, order.link) {
				if (ivisurf->prop.visibility == false)
					continue;

				tmpview = get_weston_view(ivisurf);
				assert(tmpview != NULL);

				weston_layer_entry_insert(&layout->layout_layer.view_list,
							  &tmpview->layer_link);

				ivisurf->surface->output = iviscrn->output;
			}
		}
	}
}

static void
commit_transition(struct ivi_layout* layout)
{
	if (wl_list_empty(&layout->pending_transition_list)) {
		return;
	}

	wl_list_insert_list(&layout->transitions->transition_list,
			    &layout->pending_transition_list);

	wl_list_init(&layout->pending_transition_list);

	wl_event_source_timer_update(layout->transitions->event_source, 1);
}

static void
send_surface_prop(struct ivi_layout_surface *ivisurf)
{
	wl_signal_emit(&ivisurf->property_changed, ivisurf);
	ivisurf->pending.prop.event_mask = 0;
}

static void
send_layer_prop(struct ivi_layout_layer *ivilayer)
{
	wl_signal_emit(&ivilayer->property_changed, ivilayer);
	ivilayer->pending.prop.event_mask = 0;
}

static void
send_prop(struct ivi_layout *layout)
{
	struct ivi_layout_layer   *ivilayer = NULL;
	struct ivi_layout_surface *ivisurf  = NULL;

	wl_list_for_each_reverse(ivilayer, &layout->layer_list, link) {
		if (ivilayer->prop.event_mask)
			send_layer_prop(ivilayer);
	}

	wl_list_for_each_reverse(ivisurf, &layout->surface_list, link) {
		if (ivisurf->prop.event_mask)
			send_surface_prop(ivisurf);
	}
}

static void
clear_surface_pending_list(struct ivi_layout_layer *ivilayer)
{
	struct ivi_layout_surface *surface_link = NULL;
	struct ivi_layout_surface *surface_next = NULL;

	wl_list_for_each_safe(surface_link, surface_next,
			      &ivilayer->pending.surface_list, pending.link) {
		wl_list_remove(&surface_link->pending.link);
		wl_list_init(&surface_link->pending.link);
	}
}

static void
clear_surface_order_list(struct ivi_layout_layer *ivilayer)
{
	struct ivi_layout_surface *surface_link = NULL;
	struct ivi_layout_surface *surface_next = NULL;

	wl_list_for_each_safe(surface_link, surface_next,
			      &ivilayer->order.surface_list, order.link) {
		wl_list_remove(&surface_link->order.link);
		wl_list_init(&surface_link->order.link);
		surface_link->on_layer = NULL;
	}
}

/**
 * Exported APIs of ivi-layout library are implemented from here.
 * Brief of APIs is described in ivi-layout-export.h.
 */
static int32_t
ivi_layout_add_listener_create_layer(struct wl_listener *listener)
{
	struct ivi_layout *layout = get_instance();

	if (listener == NULL) {
		weston_log("ivi_layout_add_listener_create_layer: invalid argument\n");
		return IVI_FAILED;
	}

	wl_signal_add(&layout->layer_notification.created, listener);

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_add_listener_remove_layer(struct wl_listener *listener)
{
	struct ivi_layout *layout = get_instance();

	if (listener == NULL) {
		weston_log("ivi_layout_add_listener_remove_layer: invalid argument\n");
		return IVI_FAILED;
	}

	wl_signal_add(&layout->layer_notification.removed, listener);

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_add_listener_create_surface(struct wl_listener *listener)
{
	struct ivi_layout *layout = get_instance();

	if (listener == NULL) {
		weston_log("ivi_layout_add_listener_create_surface: invalid argument\n");
		return IVI_FAILED;
	}

	wl_signal_add(&layout->surface_notification.created, listener);

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_add_listener_remove_surface(struct wl_listener *listener)
{
	struct ivi_layout *layout = get_instance();

	if (listener == NULL) {
		weston_log("ivi_layout_add_listener_remove_surface: invalid argument\n");
		return IVI_FAILED;
	}

	wl_signal_add(&layout->surface_notification.removed, listener);

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_add_listener_configure_surface(struct wl_listener *listener)
{
	struct ivi_layout *layout = get_instance();

	if (listener == NULL) {
		weston_log("ivi_layout_add_listener_configure_surface: invalid argument\n");
		return IVI_FAILED;
	}

	wl_signal_add(&layout->surface_notification.configure_changed, listener);

	return IVI_SUCCEEDED;
}

uint32_t
ivi_layout_get_id_of_surface(struct ivi_layout_surface *ivisurf)
{
	return ivisurf->id_surface;
}

static uint32_t
ivi_layout_get_id_of_layer(struct ivi_layout_layer *ivilayer)
{
	return ivilayer->id_layer;
}

static struct ivi_layout_layer *
ivi_layout_get_layer_from_id(uint32_t id_layer)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_layer *ivilayer = NULL;

	wl_list_for_each(ivilayer, &layout->layer_list, link) {
		if (ivilayer->id_layer == id_layer) {
			return ivilayer;
		}
	}

	return NULL;
}

struct ivi_layout_surface *
ivi_layout_get_surface_from_id(uint32_t id_surface)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_surface *ivisurf = NULL;

	wl_list_for_each(ivisurf, &layout->surface_list, link) {
		if (ivisurf->id_surface == id_surface) {
			return ivisurf;
		}
	}

	return NULL;
}

static int32_t
ivi_layout_surface_add_listener(struct ivi_layout_surface *ivisurf,
				    struct wl_listener *listener)
{
	if (ivisurf == NULL || listener == NULL) {
		weston_log("ivi_layout_surface_add_listener: invalid argument\n");
		return IVI_FAILED;
	}

	wl_signal_add(&ivisurf->property_changed, listener);

	return IVI_SUCCEEDED;
}

static const struct ivi_layout_layer_properties *
ivi_layout_get_properties_of_layer(struct ivi_layout_layer *ivilayer)
{
	if (ivilayer == NULL) {
		weston_log("ivi_layout_get_properties_of_layer: invalid argument\n");
		return NULL;
	}

	return &ivilayer->prop;
}

static int32_t
ivi_layout_get_screens_under_layer(struct ivi_layout_layer *ivilayer,
				   int32_t *pLength,
				   struct weston_output ***ppArray)
{
	int32_t length = 0;
	int32_t n = 0;

	if (ivilayer == NULL || pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_get_screens_under_layer: invalid argument\n");
		return IVI_FAILED;
	}

	if (ivilayer->on_screen != NULL)
		length = 1;

	if (length != 0) {
		/* the Array must be free by module which called this function */
		*ppArray = calloc(length, sizeof(struct weston_output *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		(*ppArray)[n++] = ivilayer->on_screen->output;
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_get_layers(int32_t *pLength, struct ivi_layout_layer ***ppArray)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_layer *ivilayer = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_get_layers: invalid argument\n");
		return IVI_FAILED;
	}

	length = wl_list_length(&layout->layer_list);

	if (length != 0) {
		/* the Array must be freed by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_layer *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(ivilayer, &layout->layer_list, link) {
			(*ppArray)[n++] = ivilayer;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_get_layers_on_screen(struct weston_output *output,
				int32_t *pLength,
				struct ivi_layout_layer ***ppArray)
{
	struct ivi_layout_screen *iviscrn = NULL;
	struct ivi_layout_layer *ivilayer = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (output == NULL || pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_get_layers_on_screen: invalid argument\n");
		return IVI_FAILED;
	}

	iviscrn = get_screen_from_output(output);
	length = wl_list_length(&iviscrn->order.layer_list);

	if (length != 0) {
		/* the Array must be freed by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_layer *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(ivilayer, &iviscrn->order.layer_list, order.link) {
			(*ppArray)[n++] = ivilayer;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_get_layers_under_surface(struct ivi_layout_surface *ivisurf,
				    int32_t *pLength,
				    struct ivi_layout_layer ***ppArray)
{
	int32_t length = 0;
	int32_t n = 0;

	if (ivisurf == NULL || pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_getLayers: invalid argument\n");
		return IVI_FAILED;
	}

	if (ivisurf->on_layer != NULL) {
		/* the Array must be freed by module which called this function */
		length = 1;
		*ppArray = calloc(length, sizeof(struct ivi_layout_screen *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		(*ppArray)[n++] = ivisurf->on_layer;
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

static
int32_t
ivi_layout_get_surfaces(int32_t *pLength, struct ivi_layout_surface ***ppArray)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_surface *ivisurf = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_get_surfaces: invalid argument\n");
		return IVI_FAILED;
	}

	length = wl_list_length(&layout->surface_list);

	if (length != 0) {
		/* the Array must be freed by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_surface *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(ivisurf, &layout->surface_list, link) {
			(*ppArray)[n++] = ivisurf;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_get_surfaces_on_layer(struct ivi_layout_layer *ivilayer,
				 int32_t *pLength,
				 struct ivi_layout_surface ***ppArray)
{
	struct ivi_layout_surface *ivisurf = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (ivilayer == NULL || pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_getSurfaceIDsOnLayer: invalid argument\n");
		return IVI_FAILED;
	}

	length = wl_list_length(&ivilayer->order.surface_list);

	if (length != 0) {
		/* the Array must be freed by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_surface *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(ivisurf, &ivilayer->order.surface_list, order.link) {
			(*ppArray)[n++] = ivisurf;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

static struct ivi_layout_layer *
ivi_layout_layer_create_with_dimension(uint32_t id_layer,
				       int32_t width, int32_t height)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_layer *ivilayer = NULL;

	ivilayer = get_layer(&layout->layer_list, id_layer);
	if (ivilayer != NULL) {
		weston_log("id_layer is already created\n");
		++ivilayer->ref_count;
		return ivilayer;
	}

	ivilayer = calloc(1, sizeof *ivilayer);
	if (ivilayer == NULL) {
		weston_log("fails to allocate memory\n");
		return NULL;
	}

	ivilayer->ref_count = 1;
	wl_signal_init(&ivilayer->property_changed);
	ivilayer->layout = layout;
	ivilayer->id_layer = id_layer;

	init_layer_properties(&ivilayer->prop, width, height);

	wl_list_init(&ivilayer->pending.surface_list);
	wl_list_init(&ivilayer->pending.link);
	ivilayer->pending.prop = ivilayer->prop;

	wl_list_init(&ivilayer->order.surface_list);
	wl_list_init(&ivilayer->order.link);

	wl_list_insert(&layout->layer_list, &ivilayer->link);

	wl_signal_emit(&layout->layer_notification.created, ivilayer);

	return ivilayer;
}

static void
ivi_layout_layer_destroy(struct ivi_layout_layer *ivilayer)
{
	struct ivi_layout *layout = get_instance();

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_remove: invalid argument\n");
		return;
	}

	if (--ivilayer->ref_count > 0)
		return;

	wl_signal_emit(&layout->layer_notification.removed, ivilayer);

	clear_surface_pending_list(ivilayer);
	clear_surface_order_list(ivilayer);

	wl_list_remove(&ivilayer->pending.link);
	wl_list_remove(&ivilayer->order.link);
	wl_list_remove(&ivilayer->link);

	free(ivilayer);
}

int32_t
ivi_layout_layer_set_visibility(struct ivi_layout_layer *ivilayer,
				bool newVisibility)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_visibility: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;
	prop->visibility = newVisibility;

	if (ivilayer->prop.visibility != newVisibility)
		prop->event_mask |= IVI_NOTIFICATION_VISIBILITY;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_VISIBILITY;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_layer_set_opacity(struct ivi_layout_layer *ivilayer,
			     wl_fixed_t opacity)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL ||
	    opacity < wl_fixed_from_double(0.0) ||
	    wl_fixed_from_double(1.0) < opacity) {
		weston_log("ivi_layout_layer_set_opacity: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;
	prop->opacity = opacity;

	if (ivilayer->prop.opacity != opacity)
		prop->event_mask |= IVI_NOTIFICATION_OPACITY;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_OPACITY;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_layer_set_source_rectangle(struct ivi_layout_layer *ivilayer,
				      int32_t x, int32_t y,
				      int32_t width, int32_t height)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_source_rectangle: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;
	prop->source_x = x;
	prop->source_y = y;
	prop->source_width = width;
	prop->source_height = height;

	if (ivilayer->prop.source_x != x || ivilayer->prop.source_y != y ||
	    ivilayer->prop.source_width != width ||
	    ivilayer->prop.source_height != height)
		prop->event_mask |= IVI_NOTIFICATION_SOURCE_RECT;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_SOURCE_RECT;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_layer_set_destination_rectangle(struct ivi_layout_layer *ivilayer,
					   int32_t x, int32_t y,
					   int32_t width, int32_t height)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_destination_rectangle: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;
	prop->dest_x = x;
	prop->dest_y = y;
	prop->dest_width = width;
	prop->dest_height = height;

	if (ivilayer->prop.dest_x != x || ivilayer->prop.dest_y != y ||
	    ivilayer->prop.dest_width != width ||
	    ivilayer->prop.dest_height != height)
		prop->event_mask |= IVI_NOTIFICATION_DEST_RECT;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_DEST_RECT;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_layer_set_orientation(struct ivi_layout_layer *ivilayer,
				 enum wl_output_transform orientation)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_orientation: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;
	prop->orientation = orientation;

	if (ivilayer->prop.orientation != orientation)
		prop->event_mask |= IVI_NOTIFICATION_ORIENTATION;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_ORIENTATION;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_layer_set_render_order(struct ivi_layout_layer *ivilayer,
				  struct ivi_layout_surface **pSurface,
				  int32_t number)
{
	int32_t i = 0;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_render_order: invalid argument\n");
		return IVI_FAILED;
	}

	clear_surface_pending_list(ivilayer);

	for (i = 0; i < number; i++) {
		wl_list_remove(&pSurface[i]->pending.link);
		wl_list_insert(&ivilayer->pending.surface_list,
				&pSurface[i]->pending.link);
	}

	ivilayer->order.dirty = 1;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_surface_set_visibility(struct ivi_layout_surface *ivisurf,
				  bool newVisibility)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_set_visibility: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->visibility = newVisibility;

	if (ivisurf->prop.visibility != newVisibility)
		prop->event_mask |= IVI_NOTIFICATION_VISIBILITY;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_VISIBILITY;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_surface_set_opacity(struct ivi_layout_surface *ivisurf,
			       wl_fixed_t opacity)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL ||
	    opacity < wl_fixed_from_double(0.0) ||
	    wl_fixed_from_double(1.0) < opacity) {
		weston_log("ivi_layout_surface_set_opacity: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->opacity = opacity;

	if (ivisurf->prop.opacity != opacity)
		prop->event_mask |= IVI_NOTIFICATION_OPACITY;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_OPACITY;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_surface_set_destination_rectangle(struct ivi_layout_surface *ivisurf,
					     int32_t x, int32_t y,
					     int32_t width, int32_t height)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_set_destination_rectangle: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->start_x = prop->dest_x;
	prop->start_y = prop->dest_y;
	prop->dest_x = x;
	prop->dest_y = y;
	prop->start_width = prop->dest_width;
	prop->start_height = prop->dest_height;
	prop->dest_width = width;
	prop->dest_height = height;

	if (ivisurf->prop.dest_x != x || ivisurf->prop.dest_y != y ||
	    ivisurf->prop.dest_width != width ||
	    ivisurf->prop.dest_height != height)
		prop->event_mask |= IVI_NOTIFICATION_DEST_RECT;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_DEST_RECT;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_surface_set_orientation(struct ivi_layout_surface *ivisurf,
				   enum wl_output_transform orientation)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_set_orientation: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->orientation = orientation;

	if (ivisurf->prop.orientation != orientation)
		prop->event_mask |= IVI_NOTIFICATION_ORIENTATION;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_ORIENTATION;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_screen_add_layer(struct weston_output *output,
			    struct ivi_layout_layer *addlayer)
{
	struct ivi_layout_screen *iviscrn;

	if (output == NULL || addlayer == NULL) {
		weston_log("ivi_layout_screen_add_layer: invalid argument\n");
		return IVI_FAILED;
	}

	iviscrn = get_screen_from_output(output);

	if (addlayer->on_screen == iviscrn) {
		weston_log("ivi_layout_screen_add_layer: addlayer is already available\n");
		return IVI_SUCCEEDED;
	}

	wl_list_remove(&addlayer->pending.link);
	wl_list_insert(&iviscrn->pending.layer_list, &addlayer->pending.link);

	iviscrn->order.dirty = 1;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_screen_set_render_order(struct weston_output *output,
				   struct ivi_layout_layer **pLayer,
				   const int32_t number)
{
	struct ivi_layout_screen *iviscrn;
	struct ivi_layout_layer *ivilayer = NULL;
	struct ivi_layout_layer *next = NULL;
	int32_t i = 0;

	if (output == NULL) {
		weston_log("ivi_layout_screen_set_render_order: invalid argument\n");
		return IVI_FAILED;
	}

	iviscrn = get_screen_from_output(output);

	wl_list_for_each_safe(ivilayer, next,
			      &iviscrn->pending.layer_list, pending.link) {
		wl_list_remove(&ivilayer->pending.link);
		wl_list_init(&ivilayer->pending.link);
	}

	assert(wl_list_empty(&iviscrn->pending.layer_list));

	for (i = 0; i < number; i++) {
		wl_list_remove(&pLayer[i]->pending.link);
		wl_list_insert(&iviscrn->pending.layer_list,
				&pLayer[i]->pending.link);
	}

	iviscrn->order.dirty = 1;

	return IVI_SUCCEEDED;
}

/**
 * This function is used by the additional ivi-module because of dumping ivi_surface sceenshot.
 * The ivi-module, e.g. ivi-controller.so, is in wayland-ivi-extension of Genivi's Layer Management.
 * This function is used to get the result of drawing by clients.
 */
static struct weston_surface *
ivi_layout_surface_get_weston_surface(struct ivi_layout_surface *ivisurf)
{
	return ivisurf != NULL ? ivisurf->surface : NULL;
}

static int32_t
ivi_layout_surface_get_size(struct ivi_layout_surface *ivisurf,
			    int32_t *width, int32_t *height,
			    int32_t *stride)
{
	int32_t w;
	int32_t h;
	const size_t bytespp = 4; /* PIXMAN_a8b8g8r8 */

	if (ivisurf == NULL || ivisurf->surface == NULL) {
		weston_log("%s: invalid argument\n", __func__);
		return IVI_FAILED;
	}

	weston_surface_get_content_size(ivisurf->surface, &w, &h);

	if (width != NULL)
		*width = w;

	if (height != NULL)
		*height = h;

	if (stride != NULL)
		*stride = w * bytespp;

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_layer_add_listener(struct ivi_layout_layer *ivilayer,
				  struct wl_listener *listener)
{
	if (ivilayer == NULL || listener == NULL) {
		weston_log("ivi_layout_layer_add_listener: invalid argument\n");
		return IVI_FAILED;
	}

	wl_signal_add(&ivilayer->property_changed, listener);

	return IVI_SUCCEEDED;
}

static const struct ivi_layout_surface_properties *
ivi_layout_get_properties_of_surface(struct ivi_layout_surface *ivisurf)
{
	if (ivisurf == NULL) {
		weston_log("ivi_layout_get_properties_of_surface: invalid argument\n");
		return NULL;
	}

	return &ivisurf->prop;
}

static int32_t
ivi_layout_layer_add_surface(struct ivi_layout_layer *ivilayer,
			     struct ivi_layout_surface *addsurf)
{
	if (ivilayer == NULL || addsurf == NULL) {
		weston_log("ivi_layout_layer_add_surface: invalid argument\n");
		return IVI_FAILED;
	}

	if (addsurf->on_layer == ivilayer)
		return IVI_SUCCEEDED;

	wl_list_remove(&addsurf->pending.link);
	wl_list_insert(&ivilayer->pending.surface_list, &addsurf->pending.link);

	ivilayer->order.dirty = 1;

	return IVI_SUCCEEDED;
}

static void
ivi_layout_layer_remove_surface(struct ivi_layout_layer *ivilayer,
				struct ivi_layout_surface *remsurf)
{
	if (ivilayer == NULL || remsurf == NULL) {
		weston_log("ivi_layout_layer_remove_surface: invalid argument\n");
		return;
	}

	wl_list_remove(&remsurf->pending.link);
	wl_list_init(&remsurf->pending.link);

	ivilayer->order.dirty = 1;
}

static int32_t
ivi_layout_surface_set_source_rectangle(struct ivi_layout_surface *ivisurf,
					int32_t x, int32_t y,
					int32_t width, int32_t height)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_set_source_rectangle: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->source_x = x;
	prop->source_y = y;
	prop->source_width = width;
	prop->source_height = height;

	if (ivisurf->prop.source_x != x || ivisurf->prop.source_y != y ||
	    ivisurf->prop.source_width != width ||
	    ivisurf->prop.source_height != height)
		prop->event_mask |= IVI_NOTIFICATION_SOURCE_RECT;
	else
		prop->event_mask &= ~IVI_NOTIFICATION_SOURCE_RECT;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_commit_changes(void)
{
	struct ivi_layout *layout = get_instance();

	commit_surface_list(layout);
	commit_layer_list(layout);
	commit_screen_list(layout);

	commit_transition(layout);

	commit_changes(layout);
	send_prop(layout);
	weston_compositor_schedule_repaint(layout->compositor);

	return IVI_SUCCEEDED;
}

static int32_t
ivi_layout_layer_set_transition(struct ivi_layout_layer *ivilayer,
				enum ivi_layout_transition_type type,
				uint32_t duration)
{
	if (ivilayer == NULL) {
		weston_log("%s: invalid argument\n", __func__);
		return -1;
	}

	ivilayer->pending.prop.transition_type = type;
	ivilayer->pending.prop.transition_duration = duration;

	return 0;
}

static int32_t
ivi_layout_layer_set_fade_info(struct ivi_layout_layer* ivilayer,
			       uint32_t is_fade_in,
			       double start_alpha, double end_alpha)
{
	if (ivilayer == NULL) {
		weston_log("%s: invalid argument\n", __func__);
		return -1;
	}

	ivilayer->pending.prop.is_fade_in = is_fade_in;
	ivilayer->pending.prop.start_alpha = start_alpha;
	ivilayer->pending.prop.end_alpha = end_alpha;

	return 0;
}

static int32_t
ivi_layout_surface_set_transition_duration(struct ivi_layout_surface *ivisurf,
					   uint32_t duration)
{
	struct ivi_layout_surface_properties *prop;

	if (ivisurf == NULL) {
		weston_log("%s: invalid argument\n", __func__);
		return -1;
	}

	prop = &ivisurf->pending.prop;
	prop->transition_duration = duration*10;
	return 0;
}

static int32_t
ivi_layout_surface_set_transition(struct ivi_layout_surface *ivisurf,
				  enum ivi_layout_transition_type type,
				  uint32_t duration)
{
	struct ivi_layout_surface_properties *prop;

	if (ivisurf == NULL) {
		weston_log("%s: invalid argument\n", __func__);
		return -1;
	}

	prop = &ivisurf->pending.prop;
	prop->transition_type = type;
	prop->transition_duration = duration;
	return 0;
}

static int32_t
ivi_layout_surface_dump(struct weston_surface *surface,
			void *target, size_t size,int32_t x, int32_t y,
			int32_t width, int32_t height)
{
	int result = 0;

	if (surface == NULL) {
		weston_log("%s: invalid argument\n", __func__);
		return IVI_FAILED;
	}

	result = weston_surface_copy_content(
		surface, target, size,
		x, y, width, height);

	return result == 0 ? IVI_SUCCEEDED : IVI_FAILED;
}

/**
 * methods of interaction between ivi-shell with ivi-layout
 */
struct weston_view *
ivi_layout_get_weston_view(struct ivi_layout_surface *surface)
{
	if (surface == NULL)
		return NULL;

	return get_weston_view(surface);
}

void
ivi_layout_surface_configure(struct ivi_layout_surface *ivisurf,
			     int32_t width, int32_t height)
{
	struct ivi_layout *layout = get_instance();

	/* emit callback which is set by ivi-layout api user */
	wl_signal_emit(&layout->surface_notification.configure_changed,
		       ivisurf);
}

struct ivi_layout_surface*
ivi_layout_surface_create(struct weston_surface *wl_surface,
			  uint32_t id_surface)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_surface *ivisurf = NULL;
	struct weston_view *tmpview = NULL;

	if (wl_surface == NULL) {
		weston_log("ivi_layout_surface_create: invalid argument\n");
		return NULL;
	}

	ivisurf = get_surface(&layout->surface_list, id_surface);
	if (ivisurf != NULL) {
		if (ivisurf->surface != NULL) {
			weston_log("id_surface(%d) is already created\n", id_surface);
			return NULL;
		}
	}

	ivisurf = calloc(1, sizeof *ivisurf);
	if (ivisurf == NULL) {
		weston_log("fails to allocate memory\n");
		return NULL;
	}

	wl_signal_init(&ivisurf->property_changed);
	ivisurf->id_surface = id_surface;
	ivisurf->layout = layout;

	ivisurf->surface = wl_surface;

	tmpview = weston_view_create(wl_surface);
	if (tmpview == NULL) {
		weston_log("fails to allocate memory\n");
	}

	ivisurf->surface->width_from_buffer  = 0;
	ivisurf->surface->height_from_buffer = 0;

	weston_matrix_init(&ivisurf->transform.matrix);
	wl_list_init(&ivisurf->transform.link);

	init_surface_properties(&ivisurf->prop);

	ivisurf->pending.prop = ivisurf->prop;
	wl_list_init(&ivisurf->pending.link);

	wl_list_init(&ivisurf->order.link);
	wl_list_init(&ivisurf->order.layer_list);

	wl_list_insert(&layout->surface_list, &ivisurf->link);

	wl_signal_emit(&layout->surface_notification.created, ivisurf);

	return ivisurf;
}

void
ivi_layout_init_with_compositor(struct weston_compositor *ec)
{
	struct ivi_layout *layout = get_instance();

	layout->compositor = ec;

	wl_list_init(&layout->surface_list);
	wl_list_init(&layout->layer_list);
	wl_list_init(&layout->screen_list);

	wl_signal_init(&layout->layer_notification.created);
	wl_signal_init(&layout->layer_notification.removed);

	wl_signal_init(&layout->surface_notification.created);
	wl_signal_init(&layout->surface_notification.removed);
	wl_signal_init(&layout->surface_notification.configure_changed);

	/* Add layout_layer at the last of weston_compositor.layer_list */
	weston_layer_init(&layout->layout_layer, ec->layer_list.prev);

	create_screen(ec);

	layout->transitions = ivi_layout_transition_set_create(ec);
	wl_list_init(&layout->pending_transition_list);
}

static struct ivi_layout_interface ivi_layout_interface = {
	/**
	 * commit all changes
	 */
	.commit_changes = ivi_layout_commit_changes,

	/**
	 * surface controller interfaces
	 */
	.add_listener_create_surface	= ivi_layout_add_listener_create_surface,
	.add_listener_remove_surface	= ivi_layout_add_listener_remove_surface,
	.add_listener_configure_surface	= ivi_layout_add_listener_configure_surface,
	.get_surfaces				= ivi_layout_get_surfaces,
	.get_id_of_surface			= ivi_layout_get_id_of_surface,
	.get_surface_from_id			= ivi_layout_get_surface_from_id,
	.get_properties_of_surface		= ivi_layout_get_properties_of_surface,
	.get_surfaces_on_layer			= ivi_layout_get_surfaces_on_layer,
	.surface_set_visibility			= ivi_layout_surface_set_visibility,
	.surface_set_opacity			= ivi_layout_surface_set_opacity,
	.surface_set_source_rectangle		= ivi_layout_surface_set_source_rectangle,
	.surface_set_destination_rectangle	= ivi_layout_surface_set_destination_rectangle,
	.surface_set_orientation		= ivi_layout_surface_set_orientation,
	.surface_add_listener			= ivi_layout_surface_add_listener,
	.surface_get_weston_surface		= ivi_layout_surface_get_weston_surface,
	.surface_set_transition			= ivi_layout_surface_set_transition,
	.surface_set_transition_duration	= ivi_layout_surface_set_transition_duration,

	/**
	 * layer controller interfaces
	 */
	.add_listener_create_layer			= ivi_layout_add_listener_create_layer,
	.add_listener_remove_layer		= ivi_layout_add_listener_remove_layer,
	.layer_create_with_dimension		= ivi_layout_layer_create_with_dimension,
	.layer_destroy				= ivi_layout_layer_destroy,
	.get_layers				= ivi_layout_get_layers,
	.get_id_of_layer			= ivi_layout_get_id_of_layer,
	.get_layer_from_id			= ivi_layout_get_layer_from_id,
	.get_properties_of_layer		= ivi_layout_get_properties_of_layer,
	.get_layers_under_surface		= ivi_layout_get_layers_under_surface,
	.get_layers_on_screen			= ivi_layout_get_layers_on_screen,
	.layer_set_visibility			= ivi_layout_layer_set_visibility,
	.layer_set_opacity			= ivi_layout_layer_set_opacity,
	.layer_set_source_rectangle		= ivi_layout_layer_set_source_rectangle,
	.layer_set_destination_rectangle	= ivi_layout_layer_set_destination_rectangle,
	.layer_set_orientation			= ivi_layout_layer_set_orientation,
	.layer_add_surface			= ivi_layout_layer_add_surface,
	.layer_remove_surface			= ivi_layout_layer_remove_surface,
	.layer_set_render_order			= ivi_layout_layer_set_render_order,
	.layer_add_listener			= ivi_layout_layer_add_listener,
	.layer_set_transition			= ivi_layout_layer_set_transition,

	/**
	 * screen controller interfaces
	 */
	.get_screens_under_layer	= ivi_layout_get_screens_under_layer,
	.screen_add_layer		= ivi_layout_screen_add_layer,
	.screen_set_render_order	= ivi_layout_screen_set_render_order,

	/**
	 * animation
	 */
	.transition_move_layer_cancel	= ivi_layout_transition_move_layer_cancel,
	.layer_set_fade_info		= ivi_layout_layer_set_fade_info,

	/**
	 * surface content dumping for debugging
	 */
	.surface_get_size		= ivi_layout_surface_get_size,
	.surface_dump			= ivi_layout_surface_dump,
};

int
load_controller_modules(struct weston_compositor *compositor, const char *modules,
			int *argc, char *argv[])
{
	const char *p, *end;
	char buffer[256];
	int (*controller_module_init)(struct weston_compositor *compositor,
				      int *argc, char *argv[],
				      const struct ivi_layout_interface *interface,
				      size_t interface_version);

	if (modules == NULL)
		return 0;

	p = modules;
	while (*p) {
		end = strchrnul(p, ',');
		snprintf(buffer, sizeof buffer, "%.*s", (int)(end - p), p);

		controller_module_init = weston_load_module(buffer, "controller_module_init");
		if (!controller_module_init)
			return -1;

		if (controller_module_init(compositor, argc, argv,
					   &ivi_layout_interface,
				sizeof(struct ivi_layout_interface)) != 0) {
			weston_log("ivi-shell: Initialization of controller module fails");
			return -1;
		}

		p = end;
		while (*p == ',')
			p++;
	}

	return 0;
}

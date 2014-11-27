/*
 * Copyright (C) 2013 DENSO CORPORATION
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * Implementation of ivi-layout library. The actual view on ivi_screen is
 * not updated till calling ivi_layout_commit_changes. A overview from
 * calling API for updating properties of ivi_surface/ivi_layer to asking
 * compositor to compose them by using weston_compositor_schedule_repaint,
 * 0/ initialize this library by ivi_layout_init_with_compositor
 *    with (struct weston_compositor *ec) from ivi-shell.
 * 1/ When a API for updating properties of ivi_surface/ivi_layer, it updates
 *    pending prop of ivi_surface/ivi_layer/ivi_screen which are structure to
 *    store properties.
 * 2/ Before calling commitChanges, in case of calling a API to get a property,
 *    return current property, not pending property.
 * 3/ At the timing of calling ivi_layout_commitChanges, pending properties
 *    are applied to properties.
 *
 *    *) ivi_layout_commitChanges is also called by transition animation
 *    per each frame. See ivi-layout-transition.c in details. Transition
 *    animation interpolates frames between previous properties of ivi_surface
 *    and new ones.
 *    For example, when a property of ivi_surface is changed from invisibility
 *    to visibility, it behaves like fade-in. When ivi_layout_commitChange is
 *    called during transition animation, it cancels the transition and
 *    re-start transition to new properties from current properties of final
 *    frame just before the the cancellation.
 *
 * 4/ According properties, set transformation by using weston_matrix and
 *    weston_view per ivi_surfaces and ivi_layers in while loop.
 * 5/ Set damage and trigger transform by using weston_view_geometry_dirty.
 * 6/ Notify update of properties.
 * 7/ Trigger composition by weston_compositor_schedule_repaint.
 *
 */

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>

#include "compositor.h"
#include "ivi-layout-export.h"
#include "ivi-layout-private.h"

struct link_layer {
	struct ivi_layout_layer *ivilayer;
	struct wl_list link;
	struct wl_list link_to_layer;
};

struct link_screen {
	struct ivi_layout_screen *iviscrn;
	struct wl_list link;
	struct wl_list link_to_screen;
};

struct listener_layout_notification {
	void *userdata;
	struct wl_listener listener;
};

struct ivi_layout;

struct ivi_layout_screen {
	struct wl_list link;
	struct wl_list link_to_layer;
	uint32_t id_screen;

	struct ivi_layout *layout;
	struct weston_output *output;

	uint32_t event_mask;

	struct {
		struct wl_list layer_list;
		struct wl_list link;
	} pending;

	struct {
		struct wl_list layer_list;
		struct wl_list link;
	} order;
};

struct ivi_layout_notification_callback {
	void *callback;
	void *data;
};

static struct ivi_layout ivilayout = {0};

struct ivi_layout *
get_instance(void)
{
	return &ivilayout;
}

/**
 * Internal API to add/remove a link to ivi_surface from ivi_layer.
 */
static void
add_link_to_surface(struct ivi_layout_layer *ivilayer,
		    struct link_layer *link_layer)
{
	struct link_layer *link = NULL;

	wl_list_for_each(link, &ivilayer->link_to_surface, link_to_layer) {
		if (link == link_layer)
			return;
	}

	wl_list_insert(&ivilayer->link_to_surface, &link_layer->link_to_layer);
}

static void
remove_link_to_surface(struct ivi_layout_layer *ivilayer)
{
	struct link_layer *link = NULL;
	struct link_layer *next = NULL;

	wl_list_for_each_safe(link, next, &ivilayer->link_to_surface, link_to_layer) {
		if (!wl_list_empty(&link->link_to_layer)) {
			wl_list_remove(&link->link_to_layer);
		}
		if (!wl_list_empty(&link->link)) {
			wl_list_remove(&link->link);
		}
		free(link);
	}

	wl_list_init(&ivilayer->link_to_surface);
}

/**
 * Internal API to add a link to ivi_layer from ivi_screen.
 */
static void
add_link_to_layer(struct ivi_layout_screen *iviscrn,
		  struct link_screen *link_screen)
{
	wl_list_init(&link_screen->link_to_screen);
	wl_list_insert(&iviscrn->link_to_layer, &link_screen->link_to_screen);
}

/**
 * Internal API to add/remove a ivi_surface from ivi_layer.
 */
static void
add_ordersurface_to_layer(struct ivi_layout_surface *ivisurf,
			  struct ivi_layout_layer *ivilayer)
{
	struct link_layer *link_layer = NULL;

	link_layer = malloc(sizeof *link_layer);
	if (link_layer == NULL) {
		weston_log("fails to allocate memory\n");
		return;
	}

	link_layer->ivilayer = ivilayer;
	wl_list_init(&link_layer->link);
	wl_list_insert(&ivisurf->layer_list, &link_layer->link);
	add_link_to_surface(ivilayer, link_layer);
}

static void
remove_ordersurface_from_layer(struct ivi_layout_surface *ivisurf)
{
	struct link_layer *link_layer = NULL;
	struct link_layer *next = NULL;

	wl_list_for_each_safe(link_layer, next, &ivisurf->layer_list, link) {
		if (!wl_list_empty(&link_layer->link)) {
			wl_list_remove(&link_layer->link);
		}
		if (!wl_list_empty(&link_layer->link_to_layer)) {
			wl_list_remove(&link_layer->link_to_layer);
		}
		free(link_layer);
	}
	wl_list_init(&ivisurf->layer_list);
}

/**
 * Internal API to add/remove a ivi_layer to/from ivi_screen.
 */
static void
add_orderlayer_to_screen(struct ivi_layout_layer *ivilayer,
			 struct ivi_layout_screen *iviscrn)
{
	struct link_screen *link_scrn = NULL;

	link_scrn = malloc(sizeof *link_scrn);
	if (link_scrn == NULL) {
		weston_log("fails to allocate memory\n");
		return;
	}

	link_scrn->iviscrn = iviscrn;
	wl_list_init(&link_scrn->link);
	wl_list_insert(&ivilayer->screen_list, &link_scrn->link);
	add_link_to_layer(iviscrn, link_scrn);
}

static void
remove_orderlayer_from_screen(struct ivi_layout_layer *ivilayer)
{
	struct link_screen *link_scrn = NULL;
	struct link_screen *next = NULL;

	wl_list_for_each_safe(link_scrn, next, &ivilayer->screen_list, link) {
		if (!wl_list_empty(&link_scrn->link)) {
			wl_list_remove(&link_scrn->link);
		}
		if (!wl_list_empty(&link_scrn->link_to_screen)) {
			wl_list_remove(&link_scrn->link_to_screen);
		}
		free(link_scrn);
	}
	wl_list_init(&ivilayer->screen_list);
}

/**
 * Internal API to add/remove a ivi_layer to/from ivi_screen.
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

/**
 * Called at destruction of ivi_surface
 */
static void
westonsurface_destroy_from_ivisurface(struct wl_listener *listener, void *data)
{
	struct ivi_layout_surface *ivisurf = NULL;

	ivisurf = container_of(listener, struct ivi_layout_surface,
			       surface_destroy_listener);

	wl_list_remove(&ivisurf->surface_rotation.link);
	wl_list_remove(&ivisurf->layer_rotation.link);
	wl_list_remove(&ivisurf->surface_pos.link);
	wl_list_remove(&ivisurf->layer_pos.link);
	wl_list_remove(&ivisurf->scaling.link);

	ivisurf->surface = NULL;
	ivi_layout_surface_remove(ivisurf);
}

/**
 * Internal API to check ivi_layer/ivi_surface already added in ivi_layer/ivi_screen.
 * Called by ivi_layout_layer_add_surface/ivi_layout_screenAddLayer
 */
static int
is_surface_in_layer(struct ivi_layout_surface *ivisurf,
		    struct ivi_layout_layer *ivilayer)
{
	struct ivi_layout_surface *surf = NULL;

	wl_list_for_each(surf, &ivilayer->pending.surface_list, pending.link) {
		if (surf->id_surface == ivisurf->id_surface) {
			return 1;
		}
	}

	return 0;
}

static int
is_layer_in_screen(struct ivi_layout_layer *ivilayer,
		   struct ivi_layout_screen *iviscrn)
{
	struct ivi_layout_layer *layer = NULL;

	wl_list_for_each(layer, &iviscrn->pending.layer_list, pending.link) {
		if (layer->id_layer == ivilayer->id_layer) {
			return 1;
		}
	}

	return 0;
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
	int32_t count = 0;

	wl_list_for_each(output, &ec->output_list, link) {
		iviscrn = calloc(1, sizeof *iviscrn);
		if (iviscrn == NULL) {
			weston_log("fails to allocate memory\n");
			continue;
		}

		wl_list_init(&iviscrn->link);
		iviscrn->layout = layout;

		iviscrn->id_screen = count;
		count++;

		iviscrn->output = output;
		iviscrn->event_mask = 0;

		wl_list_init(&iviscrn->pending.layer_list);
		wl_list_init(&iviscrn->pending.link);

		wl_list_init(&iviscrn->order.layer_list);
		wl_list_init(&iviscrn->order.link);

		wl_list_init(&iviscrn->link_to_layer);

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
}

/**
 * Internal APIs to be called from ivi_layout_commit_changes.
 */
static void
update_opacity(struct ivi_layout_layer *ivilayer,
	       struct ivi_layout_surface *ivisurf)
{
	double layer_alpha = wl_fixed_to_double(ivilayer->prop.opacity);
	double surf_alpha  = wl_fixed_to_double(ivisurf->prop.opacity);

	if ((ivilayer->event_mask & IVI_NOTIFICATION_OPACITY) ||
	    (ivisurf->event_mask  & IVI_NOTIFICATION_OPACITY)) {
		struct weston_view *tmpview = NULL;
		wl_list_for_each(tmpview, &ivisurf->surface->views, surface_link) {
			if (tmpview == NULL) {
				continue;
			}
			tmpview->alpha = layer_alpha * surf_alpha;
		}
	}
}

static void
update_surface_orientation(struct ivi_layout_layer *ivilayer,
			   struct ivi_layout_surface *ivisurf)
{
	struct weston_view *view;
	struct weston_matrix  *matrix = &ivisurf->surface_rotation.matrix;
	float width  = 0.0f;
	float height = 0.0f;
	float v_sin  = 0.0f;
	float v_cos  = 0.0f;
	float cx = 0.0f;
	float cy = 0.0f;
	float sx = 1.0f;
	float sy = 1.0f;

	wl_list_for_each(view, &ivisurf->surface->views, surface_link) {
		if (view != NULL) {
			break;
		}
	}

	if (view == NULL) {
		return;
	}

	if ((ivilayer->prop.dest_width == 0) ||
	    (ivilayer->prop.dest_height == 0)) {
		return;
	}
	width  = (float)ivilayer->prop.dest_width;
	height = (float)ivilayer->prop.dest_height;

	switch (ivisurf->prop.orientation) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
		v_sin = 0.0f;
		v_cos = 1.0f;
		break;
	case WL_OUTPUT_TRANSFORM_90:
		v_sin = 1.0f;
		v_cos = 0.0f;
		sx = width / height;
		sy = height / width;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		v_sin = 0.0f;
		v_cos = -1.0f;
		break;
	case WL_OUTPUT_TRANSFORM_270:
	default:
		v_sin = -1.0f;
		v_cos = 0.0f;
		sx = width / height;
		sy = height / width;
		break;
	}
	wl_list_remove(&ivisurf->surface_rotation.link);
	weston_view_geometry_dirty(view);

	weston_matrix_init(matrix);
	cx = 0.5f * width;
	cy = 0.5f * height;
	weston_matrix_translate(matrix, -cx, -cy, 0.0f);
	weston_matrix_rotate_xy(matrix, v_cos, v_sin);
	weston_matrix_scale(matrix, sx, sy, 1.0);
	weston_matrix_translate(matrix, cx, cy, 0.0f);
	wl_list_insert(&view->geometry.transformation_list,
		       &ivisurf->surface_rotation.link);

	weston_view_set_transform_parent(view, NULL);
	weston_view_update_transform(view);
}

static void
update_layer_orientation(struct ivi_layout_layer *ivilayer,
			 struct ivi_layout_surface *ivisurf)
{
	struct weston_surface *es = ivisurf->surface;
	struct weston_view    *view;
	struct weston_matrix  *matrix = &ivisurf->layer_rotation.matrix;
	struct weston_output  *output = NULL;
	float width  = 0.0f;
	float height = 0.0f;
	float v_sin  = 0.0f;
	float v_cos  = 0.0f;
	float cx = 0.0f;
	float cy = 0.0f;
	float sx = 1.0f;
	float sy = 1.0f;

	wl_list_for_each(view, &ivisurf->surface->views, surface_link) {
		if (view != NULL) {
			break;
		}
	}

	if (es == NULL || view == NULL) {
		return;
	}

	output = es->output;
	if (output == NULL) {
		return;
	}
	if ((output->width == 0) || (output->height == 0)) {
		return;
	}
	width = (float)output->width;
	height = (float)output->height;

	switch (ivilayer->prop.orientation) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
		v_sin = 0.0f;
		v_cos = 1.0f;
		break;
	case WL_OUTPUT_TRANSFORM_90:
		v_sin = 1.0f;
		v_cos = 0.0f;
		sx = width / height;
		sy = height / width;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		v_sin = 0.0f;
		v_cos = -1.0f;
		break;
	case WL_OUTPUT_TRANSFORM_270:
	default:
		v_sin = -1.0f;
		v_cos = 0.0f;
		sx = width / height;
		sy = height / width;
		break;
	}
	wl_list_remove(&ivisurf->layer_rotation.link);
	weston_view_geometry_dirty(view);

	weston_matrix_init(matrix);
	cx = 0.5f * width;
	cy = 0.5f * height;
	weston_matrix_translate(matrix, -cx, -cy, 0.0f);
	weston_matrix_rotate_xy(matrix, v_cos, v_sin);
	weston_matrix_scale(matrix, sx, sy, 1.0);
	weston_matrix_translate(matrix, cx, cy, 0.0f);
	wl_list_insert(&view->geometry.transformation_list,
		       &ivisurf->layer_rotation.link);

	weston_view_set_transform_parent(view, NULL);
	weston_view_update_transform(view);
}

static void
update_surface_position(struct ivi_layout_surface *ivisurf)
{
	struct weston_view *view;
	float tx  = (float)ivisurf->prop.dest_x;
	float ty  = (float)ivisurf->prop.dest_y;
	struct weston_matrix *matrix = &ivisurf->surface_pos.matrix;

	wl_list_for_each(view, &ivisurf->surface->views, surface_link) {
		if (view != NULL) {
			break;
		}
	}

	if (view == NULL) {
		return;
	}

	wl_list_remove(&ivisurf->surface_pos.link);

	weston_matrix_init(matrix);
	weston_matrix_translate(matrix, tx, ty, 0.0f);
	wl_list_insert(&view->geometry.transformation_list,
		       &ivisurf->surface_pos.link);

	weston_view_set_transform_parent(view, NULL);
	weston_view_update_transform(view);
}

static void
update_layer_position(struct ivi_layout_layer *ivilayer,
		      struct ivi_layout_surface *ivisurf)
{
	struct weston_view *view;
	struct weston_matrix *matrix = &ivisurf->layer_pos.matrix;
	float tx  = (float)ivilayer->prop.dest_x;
	float ty  = (float)ivilayer->prop.dest_y;

	wl_list_for_each(view, &ivisurf->surface->views, surface_link) {
		if (view != NULL) {
			break;
		}
	}

	if (view == NULL) {
		return;
	}

	wl_list_remove(&ivisurf->layer_pos.link);

	weston_matrix_init(matrix);
	weston_matrix_translate(matrix, tx, ty, 0.0f);
	wl_list_insert(&view->geometry.transformation_list,
		       &ivisurf->layer_pos.link);

	weston_view_set_transform_parent(view, NULL);
	weston_view_update_transform(view);
}

static void
update_scale(struct ivi_layout_layer *ivilayer,
	     struct ivi_layout_surface *ivisurf)
{
	struct weston_view *view;
	struct weston_matrix *matrix = &ivisurf->scaling.matrix;
	float sx = 0.0f;
	float sy = 0.0f;
	float lw = 0.0f;
	float sw = 0.0f;
	float lh = 0.0f;
	float sh = 0.0f;

	wl_list_for_each(view, &ivisurf->surface->views, surface_link) {
		if (view != NULL) {
			break;
		}
	}

	if (view == NULL) {
		return;
	}

	if (ivisurf->prop.dest_width == 0 && ivisurf->prop.dest_height == 0) {
		ivisurf->prop.dest_width  = ivisurf->surface->width_from_buffer;
		ivisurf->prop.dest_height = ivisurf->surface->height_from_buffer;
	}

	lw = ((float)ivilayer->prop.dest_width  / (float)ivilayer->prop.source_width );
	sw = ((float)ivisurf->prop.dest_width	/ (float)ivisurf->prop.source_width  );
	lh = ((float)ivilayer->prop.dest_height / (float)ivilayer->prop.source_height);
	sh = ((float)ivisurf->prop.dest_height  / (float)ivisurf->prop.source_height );
	sx = sw * lw;
	sy = sh * lh;

	wl_list_remove(&ivisurf->scaling.link);
	weston_matrix_init(matrix);
	weston_matrix_scale(matrix, sx, sy, 1.0f);

	wl_list_insert(&view->geometry.transformation_list,
		       &ivisurf->scaling.link);

	weston_view_set_transform_parent(view, NULL);
	weston_view_update_transform(view);
}

static void
update_prop(struct ivi_layout_layer *ivilayer,
	    struct ivi_layout_surface *ivisurf)
{
	if (ivilayer->event_mask | ivisurf->event_mask) {
		struct weston_view *tmpview;
		update_opacity(ivilayer, ivisurf);
		update_layer_orientation(ivilayer, ivisurf);
		update_layer_position(ivilayer, ivisurf);
		update_surface_position(ivisurf);
		update_surface_orientation(ivilayer, ivisurf);
		update_scale(ivilayer, ivisurf);

		ivisurf->update_count++;

		wl_list_for_each(tmpview, &ivisurf->surface->views, surface_link) {
			if (tmpview != NULL) {
				break;
			}
		}

		if (tmpview != NULL) {
			weston_view_geometry_dirty(tmpview);
		}

		if (ivisurf->surface != NULL) {
			weston_surface_damage(ivisurf->surface);
		}
	}
}

static void
commit_changes(struct ivi_layout *layout)
{
	struct ivi_layout_screen  *iviscrn  = NULL;
	struct ivi_layout_layer   *ivilayer = NULL;
	struct ivi_layout_surface *ivisurf  = NULL;

	wl_list_for_each(iviscrn, &layout->screen_list, link) {
		wl_list_for_each(ivilayer, &iviscrn->order.layer_list, order.link) {
			wl_list_for_each(ivisurf, &ivilayer->order.surface_list, order.link) {
				update_prop(ivilayer, ivisurf);
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
		if(ivisurf->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_VIEW_DEFAULT) {
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

			if(ivisurf->pending.prop.visibility) {
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

		} else if(ivisurf->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_VIEW_DEST_RECT_ONLY){
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

		} else if(ivisurf->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_VIEW_FADE_ONLY){
			configured = 0;
			if(ivisurf->pending.prop.visibility) {
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

			if (configured && !is_surface_transition(ivisurf))
				wl_signal_emit(&ivisurf->configured, ivisurf);
		} else {
			configured = 0;
			if (ivisurf->prop.dest_width  != ivisurf->pending.prop.dest_width ||
			    ivisurf->prop.dest_height != ivisurf->pending.prop.dest_height) {
				configured = 1;
			}

			ivisurf->prop = ivisurf->pending.prop;
			ivisurf->prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;
			ivisurf->pending.prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;

			if (configured && !is_surface_transition(ivisurf))
				wl_signal_emit(&ivisurf->configured, ivisurf);
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
		if(ivilayer->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_LAYER_MOVE) {
			ivi_layout_transition_move_layer(ivilayer, ivilayer->pending.prop.dest_x, ivilayer->pending.prop.dest_y, ivilayer->pending.prop.transition_duration);
		} else if(ivilayer->pending.prop.transition_type == IVI_LAYOUT_TRANSITION_LAYER_FADE) {
			ivi_layout_transition_fade_layer(ivilayer,ivilayer->pending.prop.is_fade_in,
							 ivilayer->pending.prop.start_alpha,ivilayer->pending.prop.end_alpha,
							 NULL, NULL,
							 ivilayer->pending.prop.transition_duration);
		}
		ivilayer->pending.prop.transition_type = IVI_LAYOUT_TRANSITION_NONE;

		ivilayer->prop = ivilayer->pending.prop;

		if (!(ivilayer->event_mask &
		      (IVI_NOTIFICATION_ADD | IVI_NOTIFICATION_REMOVE)) ) {
			continue;
		}

		if (ivilayer->event_mask & IVI_NOTIFICATION_REMOVE) {
			wl_list_for_each_safe(ivisurf, next,
				&ivilayer->order.surface_list, order.link) {
				remove_ordersurface_from_layer(ivisurf);

				if (!wl_list_empty(&ivisurf->order.link)) {
					wl_list_remove(&ivisurf->order.link);
				}

				wl_list_init(&ivisurf->order.link);
				ivisurf->event_mask |= IVI_NOTIFICATION_REMOVE;
			}

			wl_list_init(&ivilayer->order.surface_list);
		}

		if (ivilayer->event_mask & IVI_NOTIFICATION_ADD) {
			wl_list_for_each_safe(ivisurf, next,
					      &ivilayer->order.surface_list, order.link) {
				remove_ordersurface_from_layer(ivisurf);

				if (!wl_list_empty(&ivisurf->order.link)) {
					wl_list_remove(&ivisurf->order.link);
				}

				wl_list_init(&ivisurf->order.link);
			}

			wl_list_init(&ivilayer->order.surface_list);
			wl_list_for_each(ivisurf, &ivilayer->pending.surface_list,
					 pending.link) {
				if(!wl_list_empty(&ivisurf->order.link)){
					wl_list_remove(&ivisurf->order.link);
					wl_list_init(&ivisurf->order.link);
				}

				wl_list_insert(&ivilayer->order.surface_list,
					       &ivisurf->order.link);
				add_ordersurface_to_layer(ivisurf, ivilayer);
				ivisurf->event_mask |= IVI_NOTIFICATION_ADD;
			}
		}
	}
}

static void
commit_screen_list(struct ivi_layout *layout)
{
	struct ivi_layout_screen  *iviscrn  = NULL;
	struct ivi_layout_layer   *ivilayer = NULL;
	struct ivi_layout_layer   *next     = NULL;
	struct ivi_layout_surface *ivisurf  = NULL;

	wl_list_for_each(iviscrn, &layout->screen_list, link) {
		if (iviscrn->event_mask & IVI_NOTIFICATION_REMOVE) {
			wl_list_for_each_safe(ivilayer, next,
					      &iviscrn->order.layer_list, order.link) {
				remove_orderlayer_from_screen(ivilayer);

				if (!wl_list_empty(&ivilayer->order.link)) {
				    wl_list_remove(&ivilayer->order.link);
				}

				wl_list_init(&ivilayer->order.link);
				ivilayer->event_mask |= IVI_NOTIFICATION_REMOVE;
			}
		}

		if (iviscrn->event_mask & IVI_NOTIFICATION_ADD) {
			wl_list_for_each_safe(ivilayer, next,
					      &iviscrn->order.layer_list, order.link) {
				remove_orderlayer_from_screen(ivilayer);

				if (!wl_list_empty(&ivilayer->order.link)) {
					wl_list_remove(&ivilayer->order.link);
				}

				wl_list_init(&ivilayer->order.link);
			}

			wl_list_init(&iviscrn->order.layer_list);
			wl_list_for_each(ivilayer, &iviscrn->pending.layer_list,
					 pending.link) {
				wl_list_insert(&iviscrn->order.layer_list,
					       &ivilayer->order.link);
				add_orderlayer_to_screen(ivilayer, iviscrn);
				ivilayer->event_mask |= IVI_NOTIFICATION_ADD;
			}
		}

		iviscrn->event_mask = 0;

		/* Clear view list of layout ivi_layer */
		wl_list_init(&layout->layout_layer.view_list.link);

		wl_list_for_each(ivilayer, &iviscrn->order.layer_list, order.link) {
			if (ivilayer->prop.visibility == false)
				continue;

			wl_list_for_each(ivisurf, &ivilayer->order.surface_list, order.link) {
				struct weston_view *tmpview = NULL;
				wl_list_for_each(tmpview, &ivisurf->surface->views, surface_link) {
					if (tmpview != NULL) {
						break;
					}
				}

				if (ivisurf->prop.visibility == false)
					continue;
				if (ivisurf->surface == NULL || tmpview == NULL)
					continue;

				weston_layer_entry_insert(&layout->layout_layer.view_list,
							  &tmpview->layer_link);

				ivisurf->surface->output = iviscrn->output;
			}
		}

		break;
	}
}

static void
commit_transition(struct ivi_layout* layout)
{
	if(wl_list_empty(&layout->pending_transition_list)){
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
	ivisurf->event_mask = 0;
}

static void
send_layer_prop(struct ivi_layout_layer *ivilayer)
{
	wl_signal_emit(&ivilayer->property_changed, ivilayer);
	ivilayer->event_mask = 0;
}

static void
send_prop(struct ivi_layout *layout)
{
	struct ivi_layout_layer   *ivilayer = NULL;
	struct ivi_layout_surface *ivisurf  = NULL;

	wl_list_for_each_reverse(ivilayer, &layout->layer_list, link) {
		send_layer_prop(ivilayer);
	}

	wl_list_for_each_reverse(ivisurf, &layout->surface_list, link) {
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
		if (!wl_list_empty(&surface_link->pending.link)) {
			wl_list_remove(&surface_link->pending.link);
		}

		wl_list_init(&surface_link->pending.link);
	}

	ivilayer->event_mask |= IVI_NOTIFICATION_REMOVE;
}

static void
clear_surface_order_list(struct ivi_layout_layer *ivilayer)
{
	struct ivi_layout_surface *surface_link = NULL;
	struct ivi_layout_surface *surface_next = NULL;

	wl_list_for_each_safe(surface_link, surface_next,
			      &ivilayer->order.surface_list, order.link) {
		if (!wl_list_empty(&surface_link->order.link)) {
			wl_list_remove(&surface_link->order.link);
		}

		wl_list_init(&surface_link->order.link);
	}

	ivilayer->event_mask |= IVI_NOTIFICATION_REMOVE;
}

static void
layer_created(struct wl_listener *listener, void *data)
{
	struct ivi_layout_layer *ivilayer = data;

	struct listener_layout_notification *notification =
		container_of(listener,
			     struct listener_layout_notification,
			     listener);

	struct ivi_layout_notification_callback *created_callback =
		notification->userdata;

	((layer_create_notification_func)created_callback->callback)
		(ivilayer, created_callback->data);
}

static void
layer_removed(struct wl_listener *listener, void *data)
{
	struct ivi_layout_layer *ivilayer = data;

	struct listener_layout_notification *notification =
		container_of(listener,
			     struct listener_layout_notification,
			     listener);

	struct ivi_layout_notification_callback *removed_callback =
		notification->userdata;

	((layer_remove_notification_func)removed_callback->callback)
		(ivilayer, removed_callback->data);
}

static void
layer_prop_changed(struct wl_listener *listener, void *data)
{
	struct ivi_layout_layer *ivilayer = data;

	struct listener_layout_notification *layout_listener =
		container_of(listener,
			     struct listener_layout_notification,
			     listener);

	struct ivi_layout_notification_callback *prop_callback =
		layout_listener->userdata;

	((layer_property_notification_func)prop_callback->callback)
		(ivilayer, &ivilayer->prop, ivilayer->event_mask, prop_callback->data);
}

static void
surface_created(struct wl_listener *listener, void *data)
{
	struct ivi_layout_surface *ivisurface = data;

	struct listener_layout_notification *notification =
		container_of(listener,
			     struct listener_layout_notification,
			     listener);

	struct ivi_layout_notification_callback *created_callback =
		notification->userdata;

	((surface_create_notification_func)created_callback->callback)
		(ivisurface, created_callback->data);
}

static void
surface_removed(struct wl_listener *listener, void *data)
{
	struct ivi_layout_surface *ivisurface = data;

	struct listener_layout_notification *notification =
		container_of(listener,
			     struct listener_layout_notification,
			     listener);

	struct ivi_layout_notification_callback *removed_callback =
		notification->userdata;

	((surface_remove_notification_func)removed_callback->callback)
		(ivisurface, removed_callback->data);
}

static void
surface_prop_changed(struct wl_listener *listener, void *data)
{
	struct ivi_layout_surface *ivisurf = data;

	struct listener_layout_notification *layout_listener =
		container_of(listener,
			     struct listener_layout_notification,
			     listener);

	struct ivi_layout_notification_callback *prop_callback =
		layout_listener->userdata;

	((surface_property_notification_func)prop_callback->callback)
		(ivisurf, &ivisurf->prop, ivisurf->event_mask, prop_callback->data);

	ivisurf->event_mask = 0;
}

static void
surface_configure_changed(struct wl_listener *listener,
			  void *data)
{
	struct ivi_layout_surface *ivisurface = data;

	struct listener_layout_notification *notification =
		container_of(listener,
			     struct listener_layout_notification,
			     listener);

	struct ivi_layout_notification_callback *configure_changed_callback =
		notification->userdata;

	((surface_configure_notification_func)configure_changed_callback->callback)
		(ivisurface, configure_changed_callback->data);
}

static int32_t
add_notification(struct wl_signal *signal,
		 wl_notify_func_t callback,
		 void *userdata)
{
	struct listener_layout_notification *notification = NULL;

	notification = malloc(sizeof *notification);
	if (notification == NULL) {
		weston_log("fails to allocate memory\n");
		free(userdata);
		return IVI_FAILED;
	}

	notification->listener.notify = callback;
	notification->userdata = userdata;

	wl_signal_add(signal, &notification->listener);

	return IVI_SUCCEEDED;
}

static void
remove_notification(struct wl_list *listener_list, void *callback, void *userdata)
{
	struct wl_listener *listener = NULL;
	struct wl_listener *next = NULL;

	wl_list_for_each_safe(listener, next, listener_list, link) {
		struct listener_layout_notification *notification =
			container_of(listener,
				     struct listener_layout_notification,
				     listener);

		struct ivi_layout_notification_callback *notification_callback =
			notification->userdata;

		if ((notification_callback->callback != callback) ||
		    (notification_callback->data != userdata)) {
			continue;
		}

		if (!wl_list_empty(&listener->link)) {
			wl_list_remove(&listener->link);
		}

		free(notification->userdata);
		free(notification);
	}
}

static void
remove_all_notification(struct wl_list *listener_list)
{
	struct wl_listener *listener = NULL;
	struct wl_listener *next = NULL;

	wl_list_for_each_safe(listener, next, listener_list, link) {
		struct listener_layout_notification *notification = NULL;
		if (!wl_list_empty(&listener->link)) {
			wl_list_remove(&listener->link);
		}

		notification =
			container_of(listener,
				     struct listener_layout_notification,
				     listener);

		free(notification->userdata);
		free(notification);
	}
}

/**
 * Exported APIs of ivi-layout library are implemented from here.
 * Brief of APIs is described in ivi-layout-export.h.
 */
WL_EXPORT int32_t
ivi_layout_add_notification_create_layer(layer_create_notification_func callback,
					 void *userdata)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_notification_callback *created_callback = NULL;

	if (callback == NULL) {
		weston_log("ivi_layout_add_notification_create_layer: invalid argument\n");
		return IVI_FAILED;
	}

	created_callback = malloc(sizeof *created_callback);
	if (created_callback == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	created_callback->callback = callback;
	created_callback->data = userdata;

	return add_notification(&layout->layer_notification.created,
				layer_created,
				created_callback);
}

WL_EXPORT void
ivi_layout_remove_notification_create_layer(layer_create_notification_func callback,
					    void *userdata)
{
	struct ivi_layout *layout = get_instance();
	remove_notification(&layout->layer_notification.created.listener_list, callback, userdata);
}

WL_EXPORT int32_t
ivi_layout_add_notification_remove_layer(layer_remove_notification_func callback,
					 void *userdata)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_notification_callback *removed_callback = NULL;

	if (callback == NULL) {
		weston_log("ivi_layout_add_notification_remove_layer: invalid argument\n");
		return IVI_FAILED;
	}

	removed_callback = malloc(sizeof *removed_callback);
	if (removed_callback == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	removed_callback->callback = callback;
	removed_callback->data = userdata;
	return add_notification(&layout->layer_notification.removed,
				layer_removed,
				removed_callback);
}

WL_EXPORT void
ivi_layout_remove_notification_remove_layer(layer_remove_notification_func callback,
					    void *userdata)
{
	struct ivi_layout *layout = get_instance();
	remove_notification(&layout->layer_notification.removed.listener_list, callback, userdata);
}

WL_EXPORT int32_t
ivi_layout_add_notification_create_surface(surface_create_notification_func callback,
					   void *userdata)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_notification_callback *created_callback = NULL;

	if (callback == NULL) {
		weston_log("ivi_layout_add_notification_create_surface: invalid argument\n");
		return IVI_FAILED;
	}

	created_callback = malloc(sizeof *created_callback);
	if (created_callback == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	created_callback->callback = callback;
	created_callback->data = userdata;

	return add_notification(&layout->surface_notification.created,
				surface_created,
				created_callback);
}

WL_EXPORT void
ivi_layout_remove_notification_create_surface(surface_create_notification_func callback,
					      void *userdata)
{
	struct ivi_layout *layout = get_instance();
	remove_notification(&layout->surface_notification.created.listener_list, callback, userdata);
}

WL_EXPORT int32_t
ivi_layout_add_notification_remove_surface(surface_remove_notification_func callback,
					   void *userdata)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_notification_callback *removed_callback = NULL;

	if (callback == NULL) {
		weston_log("ivi_layout_add_notification_remove_surface: invalid argument\n");
		return IVI_FAILED;
	}

	removed_callback = malloc(sizeof *removed_callback);
	if (removed_callback == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	removed_callback->callback = callback;
	removed_callback->data = userdata;

	return add_notification(&layout->surface_notification.removed,
				surface_removed,
				removed_callback);
}

WL_EXPORT void
ivi_layout_remove_notification_remove_surface(surface_remove_notification_func callback,
					      void *userdata)
{
	struct ivi_layout *layout = get_instance();
	remove_notification(&layout->surface_notification.removed.listener_list, callback, userdata);
}

WL_EXPORT int32_t
ivi_layout_add_notification_configure_surface(surface_configure_notification_func callback,
					      void *userdata)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_notification_callback *configure_changed_callback = NULL;
	if (callback == NULL) {
		weston_log("ivi_layout_add_notification_configure_surface: invalid argument\n");
		return IVI_FAILED;
	}

	configure_changed_callback = malloc(sizeof *configure_changed_callback);
	if (configure_changed_callback == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	configure_changed_callback->callback = callback;
	configure_changed_callback->data = userdata;

	return add_notification(&layout->surface_notification.configure_changed,
				surface_configure_changed,
				configure_changed_callback);
}

WL_EXPORT void
ivi_layout_remove_notification_configure_surface(surface_configure_notification_func callback,
						 void *userdata)
{
	struct ivi_layout *layout = get_instance();
	remove_notification(&layout->surface_notification.configure_changed.listener_list, callback, userdata);
}

WL_EXPORT uint32_t
ivi_layout_get_id_of_surface(struct ivi_layout_surface *ivisurf)
{
	return ivisurf->id_surface;
}

WL_EXPORT uint32_t
ivi_layout_get_id_of_layer(struct ivi_layout_layer *ivilayer)
{
	return ivilayer->id_layer;
}

struct ivi_layout_layer *
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

WL_EXPORT struct ivi_layout_surface *
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

WL_EXPORT struct ivi_layout_screen *
ivi_layout_get_screen_from_id(uint32_t id_screen)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_screen *iviscrn = NULL;

	wl_list_for_each(iviscrn, &layout->screen_list, link) {
/* FIXME : select iviscrn from screen_list by id_screen */
		return iviscrn;
		break;
	}

	return NULL;
}

WL_EXPORT int32_t
ivi_layout_get_screen_resolution(struct ivi_layout_screen *iviscrn,
				 int32_t *pWidth, int32_t *pHeight)
{
	struct weston_output *output = NULL;

	if (pWidth == NULL || pHeight == NULL) {
		weston_log("ivi_layout_get_screen_resolution: invalid argument\n");
		return IVI_FAILED;
	}

	output   = iviscrn->output;
	*pWidth  = output->current_mode->width;
	*pHeight = output->current_mode->height;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
ivi_layout_surface_add_notification(struct ivi_layout_surface *ivisurf,
				    surface_property_notification_func callback,
				    void *userdata)
{
	struct listener_layout_notification* notification = NULL;
	struct ivi_layout_notification_callback *prop_callback = NULL;

	if (ivisurf == NULL || callback == NULL) {
		weston_log("ivi_layout_surface_add_notification: invalid argument\n");
		return IVI_FAILED;
	}

	notification = malloc(sizeof *notification);
	if (notification == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	prop_callback = malloc(sizeof *prop_callback);
	if (prop_callback == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	prop_callback->callback = callback;
	prop_callback->data = userdata;

	notification->listener.notify = surface_prop_changed;
	notification->userdata = prop_callback;

	wl_signal_add(&ivisurf->property_changed, &notification->listener);

	return IVI_SUCCEEDED;
}

WL_EXPORT void
ivi_layout_surface_remove_notification(struct ivi_layout_surface *ivisurf)
{
	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_remove_notification: invalid argument\n");
		return;
	}

	remove_all_notification(&ivisurf->property_changed.listener_list);
}

static void
remove_configured_listener(struct ivi_layout_surface *ivisurf)
{
	struct wl_listener *link = NULL;
	struct wl_listener *next = NULL;

	wl_list_for_each_safe(link, next, &ivisurf->configured.listener_list, link) {
		wl_list_remove(&link->link);
	}
}

void
ivi_layout_surface_remove(struct ivi_layout_surface *ivisurf)
{
	struct ivi_layout *layout = get_instance();

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_remove: invalid argument\n");
		return;
	}

	if (!wl_list_empty(&ivisurf->pending.link)) {
		wl_list_remove(&ivisurf->pending.link);
	}
	if (!wl_list_empty(&ivisurf->order.link)) {
		wl_list_remove(&ivisurf->order.link);
	}
	if (!wl_list_empty(&ivisurf->link)) {
		wl_list_remove(&ivisurf->link);
	}
	remove_ordersurface_from_layer(ivisurf);

	wl_signal_emit(&layout->surface_notification.removed, ivisurf);

	remove_configured_listener(ivisurf);

	ivi_layout_surface_remove_notification(ivisurf);

	free(ivisurf);
}

WL_EXPORT const struct ivi_layout_layer_properties *
ivi_layout_get_properties_of_layer(struct ivi_layout_layer *ivilayer)
{
	if (ivilayer == NULL) {
		weston_log("ivi_layout_get_properties_of_layer: invalid argument\n");
		return NULL;
	}

	return &ivilayer->prop;
}

WL_EXPORT int32_t
ivi_layout_get_screens(int32_t *pLength, struct ivi_layout_screen ***ppArray)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_screen *iviscrn = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_get_screens: invalid argument\n");
		return IVI_FAILED;
	}

	length = wl_list_length(&layout->screen_list);

	if (length != 0){
		/* the Array must be free by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_screen *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(iviscrn, &layout->screen_list, link) {
			(*ppArray)[n++] = iviscrn;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
ivi_layout_get_screens_under_layer(struct ivi_layout_layer *ivilayer,
				   int32_t *pLength,
				   struct ivi_layout_screen ***ppArray)
{
	struct link_screen *link_scrn = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (ivilayer == NULL || pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_get_screens_under_layer: invalid argument\n");
		return IVI_FAILED;
	}

	length = wl_list_length(&ivilayer->screen_list);

	if (length != 0){
		/* the Array must be free by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_screen *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(link_scrn, &ivilayer->screen_list, link) {
			(*ppArray)[n++] = link_scrn->iviscrn;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
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

	if (length != 0){
		/* the Array must be free by module which called this function */
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

int32_t
ivi_layout_get_layers_on_screen(struct ivi_layout_screen *iviscrn,
				int32_t *pLength,
				struct ivi_layout_layer ***ppArray)
{
	struct ivi_layout_layer *ivilayer = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (iviscrn == NULL || pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_get_layers_on_screen: invalid argument\n");
		return IVI_FAILED;
	}

	length = wl_list_length(&iviscrn->order.layer_list);

	if (length != 0){
		/* the Array must be free by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_layer *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(ivilayer, &iviscrn->order.layer_list, link) {
			(*ppArray)[n++] = ivilayer;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
ivi_layout_get_layers_under_surface(struct ivi_layout_surface *ivisurf,
				    int32_t *pLength,
				    struct ivi_layout_layer ***ppArray)
{
	struct link_layer *link_layer = NULL;
	int32_t length = 0;
	int32_t n = 0;

	if (ivisurf == NULL || pLength == NULL || ppArray == NULL) {
		weston_log("ivi_layout_getLayers: invalid argument\n");
		return IVI_FAILED;
	}

	length = wl_list_length(&ivisurf->layer_list);

	if (length != 0){
		/* the Array must be free by module which called this function */
		*ppArray = calloc(length, sizeof(struct ivi_layout_layer *));
		if (*ppArray == NULL) {
			weston_log("fails to allocate memory\n");
			return IVI_FAILED;
		}

		wl_list_for_each(link_layer, &ivisurf->layer_list, link) {
			(*ppArray)[n++] = link_layer->ivilayer;
		}
	}

	*pLength = length;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
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

	if (length != 0){
		/* the Array must be free by module which called this function */
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

int32_t
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
		/* the Array must be free by module which called this function */
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

WL_EXPORT struct ivi_layout_layer *
ivi_layout_layer_create_with_dimension(uint32_t id_layer,
				       int32_t width, int32_t height)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_layer *ivilayer = NULL;

	ivilayer = get_layer(&layout->layer_list, id_layer);
	if (ivilayer != NULL) {
		weston_log("id_layer is already created\n");
		return ivilayer;
	}

	ivilayer = calloc(1, sizeof *ivilayer);
	if (ivilayer == NULL) {
		weston_log("fails to allocate memory\n");
		return NULL;
	}

	wl_list_init(&ivilayer->link);
	wl_signal_init(&ivilayer->property_changed);
	wl_list_init(&ivilayer->screen_list);
	wl_list_init(&ivilayer->link_to_surface);
	ivilayer->layout = layout;
	ivilayer->id_layer = id_layer;

	init_layer_properties(&ivilayer->prop, width, height);
	ivilayer->event_mask = 0;

	wl_list_init(&ivilayer->pending.surface_list);
	wl_list_init(&ivilayer->pending.link);
	ivilayer->pending.prop = ivilayer->prop;

	wl_list_init(&ivilayer->order.surface_list);
	wl_list_init(&ivilayer->order.link);

	wl_list_insert(&layout->layer_list, &ivilayer->link);

	wl_signal_emit(&layout->layer_notification.created, ivilayer);

	return ivilayer;
}

WL_EXPORT void
ivi_layout_layer_remove(struct ivi_layout_layer *ivilayer)
{
	struct ivi_layout *layout = get_instance();

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_remove: invalid argument\n");
		return;
	}

	wl_signal_emit(&layout->layer_notification.removed, ivilayer);

	clear_surface_pending_list(ivilayer);
	clear_surface_order_list(ivilayer);

	if (!wl_list_empty(&ivilayer->pending.link)) {
		wl_list_remove(&ivilayer->pending.link);
	}
	if (!wl_list_empty(&ivilayer->order.link)) {
		wl_list_remove(&ivilayer->order.link);
	}
	if (!wl_list_empty(&ivilayer->link)) {
		wl_list_remove(&ivilayer->link);
	}
	remove_orderlayer_from_screen(ivilayer);
	remove_link_to_surface(ivilayer);
	ivi_layout_layer_remove_notification(ivilayer);

	free(ivilayer);
}

WL_EXPORT int32_t
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

	ivilayer->event_mask |= IVI_NOTIFICATION_VISIBILITY;

	return IVI_SUCCEEDED;
}

bool
ivi_layout_layer_get_visibility(struct ivi_layout_layer *ivilayer)
{
	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_get_visibility: invalid argument\n");
		return false;
	}

	return ivilayer->prop.visibility;
}

WL_EXPORT int32_t
ivi_layout_layer_set_opacity(struct ivi_layout_layer *ivilayer,
			     wl_fixed_t opacity)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_opacity: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;
	prop->opacity = opacity;

	ivilayer->event_mask |= IVI_NOTIFICATION_OPACITY;

	return IVI_SUCCEEDED;
}

WL_EXPORT wl_fixed_t
ivi_layout_layer_get_opacity(struct ivi_layout_layer *ivilayer)
{
	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_get_opacity: invalid argument\n");
		return wl_fixed_from_double(0.0);
	}

	return ivilayer->prop.opacity;
}

WL_EXPORT int32_t
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

	ivilayer->event_mask |= IVI_NOTIFICATION_SOURCE_RECT;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
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

	ivilayer->event_mask |= IVI_NOTIFICATION_DEST_RECT;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_layer_get_dimension(struct ivi_layout_layer *ivilayer,
			       int32_t *dest_width, int32_t *dest_height)
{
	if (ivilayer == NULL || dest_width == NULL || dest_height == NULL) {
		weston_log("ivi_layout_layer_get_dimension: invalid argument\n");
		return IVI_FAILED;
	}

	*dest_width = ivilayer->prop.dest_width;
	*dest_height = ivilayer->prop.dest_height;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_layer_set_dimension(struct ivi_layout_layer *ivilayer,
			       int32_t dest_width, int32_t dest_height)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_dimension: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;

	prop->dest_width  = dest_width;
	prop->dest_height = dest_height;

	ivilayer->event_mask |= IVI_NOTIFICATION_DIMENSION;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
ivi_layout_layer_get_position(struct ivi_layout_layer *ivilayer,
			      int32_t *dest_x, int32_t *dest_y)
{
	if (ivilayer == NULL || dest_x == NULL || dest_y == NULL) {
		weston_log("ivi_layout_layer_get_position: invalid argument\n");
		return IVI_FAILED;
	}

	*dest_x = ivilayer->prop.dest_x;
	*dest_y = ivilayer->prop.dest_y;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
ivi_layout_layer_set_position(struct ivi_layout_layer *ivilayer,
			      int32_t dest_x, int32_t dest_y)
{
	struct ivi_layout_layer_properties *prop = NULL;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_position: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivilayer->pending.prop;
	prop->dest_x = dest_x;
	prop->dest_y = dest_y;

	ivilayer->event_mask |= IVI_NOTIFICATION_POSITION;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
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

	ivilayer->event_mask |= IVI_NOTIFICATION_ORIENTATION;

	return IVI_SUCCEEDED;
}

enum wl_output_transform
ivi_layout_layer_get_orientation(struct ivi_layout_layer *ivilayer)
{
	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_get_orientation: invalid argument\n");
		return 0;
	}

	return ivilayer->prop.orientation;
}

WL_EXPORT int32_t
ivi_layout_layer_set_render_order(struct ivi_layout_layer *ivilayer,
				  struct ivi_layout_surface **pSurface,
				  int32_t number)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_surface *ivisurf = NULL;
	struct ivi_layout_surface *next = NULL;
	uint32_t *id_surface = NULL;
	int32_t i = 0;

	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_set_render_order: invalid argument\n");
		return IVI_FAILED;
	}

	if (pSurface == NULL) {
		wl_list_for_each_safe(ivisurf, next, &ivilayer->pending.surface_list, pending.link) {
			if (!wl_list_empty(&ivisurf->pending.link)) {
				wl_list_remove(&ivisurf->pending.link);
			}

			wl_list_init(&ivisurf->pending.link);
		}
		ivilayer->event_mask |= IVI_NOTIFICATION_REMOVE;
		return IVI_SUCCEEDED;
	}

	for (i = 0; i < number; i++) {
		id_surface = &pSurface[i]->id_surface;

		wl_list_for_each_safe(ivisurf, next, &layout->surface_list, link) {
			if (*id_surface != ivisurf->id_surface) {
				continue;
			}

			if (!wl_list_empty(&ivisurf->pending.link)) {
				wl_list_remove(&ivisurf->pending.link);
			}
			wl_list_init(&ivisurf->pending.link);
			wl_list_insert(&ivilayer->pending.surface_list,
				       &ivisurf->pending.link);
			break;
		}
	}

	ivilayer->event_mask |= IVI_NOTIFICATION_ADD;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
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

	ivisurf->event_mask |= IVI_NOTIFICATION_VISIBILITY;

	return IVI_SUCCEEDED;
}

WL_EXPORT bool
ivi_layout_surface_get_visibility(struct ivi_layout_surface *ivisurf)
{
	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_get_visibility: invalid argument\n");
		return false;
	}

	return ivisurf->prop.visibility;
}

WL_EXPORT int32_t
ivi_layout_surface_set_opacity(struct ivi_layout_surface *ivisurf,
			       wl_fixed_t opacity)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_set_opacity: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->opacity = opacity;

	ivisurf->event_mask |= IVI_NOTIFICATION_OPACITY;

	return IVI_SUCCEEDED;
}

WL_EXPORT wl_fixed_t
ivi_layout_surface_get_opacity(struct ivi_layout_surface *ivisurf)
{
	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_get_opacity: invalid argument\n");
		return wl_fixed_from_double(0.0);
	}

	return ivisurf->prop.opacity;
}

WL_EXPORT int32_t
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

	ivisurf->event_mask |= IVI_NOTIFICATION_DEST_RECT;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_surface_set_dimension(struct ivi_layout_surface *ivisurf,
				 int32_t dest_width, int32_t dest_height)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_set_dimension: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->dest_width  = dest_width;
	prop->dest_height = dest_height;

	ivisurf->event_mask |= IVI_NOTIFICATION_DIMENSION;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_surface_get_dimension(struct ivi_layout_surface *ivisurf,
				 int32_t *dest_width, int32_t *dest_height)
{
	if (ivisurf == NULL || dest_width == NULL ||  dest_height == NULL) {
		weston_log("ivi_layout_surface_get_dimension: invalid argument\n");
		return IVI_FAILED;
	}

	*dest_width = ivisurf->prop.dest_width;
	*dest_height = ivisurf->prop.dest_height;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_surface_set_position(struct ivi_layout_surface *ivisurf,
				int32_t dest_x, int32_t dest_y)
{
	struct ivi_layout_surface_properties *prop = NULL;

	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_set_position: invalid argument\n");
		return IVI_FAILED;
	}

	prop = &ivisurf->pending.prop;
	prop->dest_x = dest_x;
	prop->dest_y = dest_y;

	ivisurf->event_mask |= IVI_NOTIFICATION_POSITION;

	return IVI_SUCCEEDED;
}

int32_t
ivi_layout_surface_get_position(struct ivi_layout_surface *ivisurf,
				int32_t *dest_x, int32_t *dest_y)
{
	if (ivisurf == NULL || dest_x == NULL || dest_y == NULL) {
		weston_log("ivi_layout_surface_get_position: invalid argument\n");
		return IVI_FAILED;
	}

	*dest_x = ivisurf->prop.dest_x;
	*dest_y = ivisurf->prop.dest_y;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
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

	ivisurf->event_mask |= IVI_NOTIFICATION_ORIENTATION;

	return IVI_SUCCEEDED;
}

enum wl_output_transform
ivi_layout_surface_get_orientation(struct ivi_layout_surface *ivisurf)
{
	if (ivisurf == NULL) {
		weston_log("ivi_layout_surface_get_orientation: invalid argument\n");
		return 0;
	}

	return ivisurf->prop.orientation;
}

WL_EXPORT int32_t
ivi_layout_screen_add_layer(struct ivi_layout_screen *iviscrn,
			    struct ivi_layout_layer *addlayer)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_layer *ivilayer = NULL;
	struct ivi_layout_layer *next = NULL;
	int is_layer_in_scrn = 0;

	if (iviscrn == NULL || addlayer == NULL) {
		weston_log("ivi_layout_screen_add_layer: invalid argument\n");
		return IVI_FAILED;
	}

	is_layer_in_scrn = is_layer_in_screen(addlayer, iviscrn);
	if (is_layer_in_scrn == 1) {
		weston_log("ivi_layout_screen_add_layer: addlayer is already available\n");
		return IVI_SUCCEEDED;
	}

	wl_list_for_each_safe(ivilayer, next, &layout->layer_list, link) {
		if (ivilayer->id_layer == addlayer->id_layer) {
			if (!wl_list_empty(&ivilayer->pending.link)) {
				wl_list_remove(&ivilayer->pending.link);
			}
			wl_list_init(&ivilayer->pending.link);
			wl_list_insert(&iviscrn->pending.layer_list,
				       &ivilayer->pending.link);
			break;
		}
	}

	iviscrn->event_mask |= IVI_NOTIFICATION_ADD;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
ivi_layout_screen_set_render_order(struct ivi_layout_screen *iviscrn,
				   struct ivi_layout_layer **pLayer,
				   const int32_t number)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_layer *ivilayer = NULL;
	struct ivi_layout_layer *next = NULL;
	uint32_t *id_layer = NULL;
	int32_t i = 0;

	if (iviscrn == NULL) {
		weston_log("ivi_layout_screen_set_render_order: invalid argument\n");
		return IVI_FAILED;
	}

	wl_list_for_each_safe(ivilayer, next,
			      &iviscrn->pending.layer_list, pending.link) {
		wl_list_init(&ivilayer->pending.link);
	}

	wl_list_init(&iviscrn->pending.layer_list);

	if (pLayer == NULL) {
		wl_list_for_each_safe(ivilayer, next, &iviscrn->pending.layer_list, pending.link) {
			if (!wl_list_empty(&ivilayer->pending.link)) {
				wl_list_remove(&ivilayer->pending.link);
			}

			wl_list_init(&ivilayer->pending.link);
		}

		iviscrn->event_mask |= IVI_NOTIFICATION_REMOVE;
		return IVI_SUCCEEDED;
	}

	for (i = 0; i < number; i++) {
		id_layer = &pLayer[i]->id_layer;
		wl_list_for_each(ivilayer, &layout->layer_list, link) {
			if (*id_layer != ivilayer->id_layer) {
				continue;
			}

			if (!wl_list_empty(&ivilayer->pending.link)) {
				wl_list_remove(&ivilayer->pending.link);
			}
			wl_list_init(&ivilayer->pending.link);
			wl_list_insert(&iviscrn->pending.layer_list,
				       &ivilayer->pending.link);
			break;
		}
	}

	iviscrn->event_mask |= IVI_NOTIFICATION_ADD;

	return IVI_SUCCEEDED;
}

WL_EXPORT struct weston_output *
ivi_layout_screen_get_output(struct ivi_layout_screen *iviscrn)
{
	return iviscrn->output;
}

/**
 * This function is used by the additional ivi-module because of dumping ivi_surface sceenshot.
 * The ivi-module, e.g. ivi-controller.so, is in wayland-ivi-extension of Genivi's Layer Management.
 * This function is used to get the result of drawing by clients.
 */
WL_EXPORT struct weston_surface *
ivi_layout_surface_get_weston_surface(struct ivi_layout_surface *ivisurf)
{
	return ivisurf != NULL ? ivisurf->surface : NULL;
}

WL_EXPORT int32_t
ivi_layout_layer_add_notification(struct ivi_layout_layer *ivilayer,
				  layer_property_notification_func callback,
				  void *userdata)
{
	struct ivi_layout_notification_callback *prop_callback = NULL;

	if (ivilayer == NULL || callback == NULL) {
		weston_log("ivi_layout_layer_add_notification: invalid argument\n");
		return IVI_FAILED;
	}

	prop_callback = malloc(sizeof *prop_callback);
	if (prop_callback == NULL) {
		weston_log("fails to allocate memory\n");
		return IVI_FAILED;
	}

	prop_callback->callback = callback;
	prop_callback->data = userdata;

	return add_notification(&ivilayer->property_changed,
				layer_prop_changed,
				prop_callback);
}

WL_EXPORT void
ivi_layout_layer_remove_notification(struct ivi_layout_layer *ivilayer)
{
	if (ivilayer == NULL) {
		weston_log("ivi_layout_layer_remove_notification: invalid argument\n");
		return;
	}

	remove_all_notification(&ivilayer->property_changed.listener_list);
}

WL_EXPORT const struct ivi_layout_surface_properties *
ivi_layout_get_properties_of_surface(struct ivi_layout_surface *ivisurf)
{
	if (ivisurf == NULL) {
		weston_log("ivi_layout_get_properties_of_surface: invalid argument\n");
		return NULL;
	}

	return &ivisurf->prop;
}

WL_EXPORT int32_t
ivi_layout_layer_add_surface(struct ivi_layout_layer *ivilayer,
			     struct ivi_layout_surface *addsurf)
{
	struct ivi_layout *layout = get_instance();
	struct ivi_layout_surface *ivisurf = NULL;
	struct ivi_layout_surface *next = NULL;
	int is_surf_in_layer = 0;

	if (ivilayer == NULL || addsurf == NULL) {
		weston_log("ivi_layout_layer_add_surface: invalid argument\n");
		return IVI_FAILED;
	}

	is_surf_in_layer = is_surface_in_layer(addsurf, ivilayer);
	if (is_surf_in_layer == 1) {
		weston_log("ivi_layout_layer_add_surface: addsurf is already available\n");
		return IVI_SUCCEEDED;
	}

	wl_list_for_each_safe(ivisurf, next, &layout->surface_list, link) {
		if (ivisurf->id_surface == addsurf->id_surface) {
			if (!wl_list_empty(&ivisurf->pending.link)) {
				wl_list_remove(&ivisurf->pending.link);
			}
			wl_list_init(&ivisurf->pending.link);
			wl_list_insert(&ivilayer->pending.surface_list,
				       &ivisurf->pending.link);
			break;
		}
	}

	ivilayer->event_mask |= IVI_NOTIFICATION_ADD;

	return IVI_SUCCEEDED;
}

WL_EXPORT void
ivi_layout_layer_remove_surface(struct ivi_layout_layer *ivilayer,
				struct ivi_layout_surface *remsurf)
{
	struct ivi_layout_surface *ivisurf = NULL;
	struct ivi_layout_surface *next = NULL;

	if (ivilayer == NULL || remsurf == NULL) {
		weston_log("ivi_layout_layer_remove_surface: invalid argument\n");
		return;
	}

	wl_list_for_each_safe(ivisurf, next,
			      &ivilayer->pending.surface_list, pending.link) {
		if (ivisurf->id_surface == remsurf->id_surface) {
			if (!wl_list_empty(&ivisurf->pending.link)) {
				wl_list_remove(&ivisurf->pending.link);
			}
			wl_list_init(&ivisurf->pending.link);
			break;
		}
	}

	remsurf->event_mask |= IVI_NOTIFICATION_REMOVE;
}

WL_EXPORT int32_t
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

	ivisurf->event_mask |= IVI_NOTIFICATION_SOURCE_RECT;

	return IVI_SUCCEEDED;
}

WL_EXPORT int32_t
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

/***called from ivi-shell**/
static struct weston_view *
ivi_layout_get_weston_view(struct ivi_layout_surface *surface)
{
	struct weston_view *tmpview = NULL;

	if(surface == NULL)
		return NULL;

	wl_list_for_each(tmpview, &surface->surface->views, surface_link)
	{
		if (tmpview != NULL) {
			break;
		}
	}
	return tmpview;
}

static void
ivi_layout_surface_configure(struct ivi_layout_surface *ivisurf,
			     int32_t width, int32_t height)
{
	struct ivi_layout *layout = get_instance();
	int32_t in_init = 0;
	ivisurf->surface->width_from_buffer  = width;
	ivisurf->surface->height_from_buffer = height;

	if (ivisurf->prop.source_width == 0 || ivisurf->prop.source_height == 0) {
		in_init = 1;
	}

	/* FIXME: when sourceHeight/Width is used as clipping range in image buffer */
	/* if (ivisurf->prop.sourceWidth == 0 || ivisurf->prop.sourceHeight == 0) { */
		ivisurf->pending.prop.source_width = width;
		ivisurf->pending.prop.source_height = height;
		ivisurf->prop.source_width = width;
		ivisurf->prop.source_height = height;
	/* } */

	ivisurf->event_mask |= IVI_NOTIFICATION_CONFIGURE;

	if (in_init) {
		wl_signal_emit(&layout->surface_notification.configure_changed, ivisurf);
	} else {
		ivi_layout_commit_changes();
	}
}

WL_EXPORT int32_t
ivi_layout_surface_set_content_observer(struct ivi_layout_surface *ivisurf,
					ivi_controller_surface_content_callback callback,
					void* userdata)
{
	int32_t ret = IVI_FAILED;

	if (ivisurf != NULL) {
		ivisurf->content_observer.callback = callback;
		ivisurf->content_observer.userdata = userdata;
		ret = IVI_SUCCEEDED;
	}
	return ret;
}

static struct ivi_layout_surface*
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

	wl_list_init(&ivisurf->link);
	wl_signal_init(&ivisurf->property_changed);
	wl_signal_init(&ivisurf->configured);
	wl_list_init(&ivisurf->layer_list);
	ivisurf->id_surface = id_surface;
	ivisurf->layout = layout;

	ivisurf->surface = wl_surface;
	ivisurf->surface_destroy_listener.notify =
		westonsurface_destroy_from_ivisurface;
	wl_resource_add_destroy_listener(wl_surface->resource,
					 &ivisurf->surface_destroy_listener);

	tmpview = weston_view_create(wl_surface);
	if (tmpview == NULL) {
		weston_log("fails to allocate memory\n");
	}

	ivisurf->surface->width_from_buffer  = 0;
	ivisurf->surface->height_from_buffer = 0;

	weston_matrix_init(&ivisurf->surface_rotation.matrix);
	weston_matrix_init(&ivisurf->layer_rotation.matrix);
	weston_matrix_init(&ivisurf->surface_pos.matrix);
	weston_matrix_init(&ivisurf->layer_pos.matrix);
	weston_matrix_init(&ivisurf->scaling.matrix);

	wl_list_init(&ivisurf->surface_rotation.link);
	wl_list_init(&ivisurf->layer_rotation.link);
	wl_list_init(&ivisurf->surface_pos.link);
	wl_list_init(&ivisurf->layer_pos.link);
	wl_list_init(&ivisurf->scaling.link);

	init_surface_properties(&ivisurf->prop);
	ivisurf->event_mask = 0;

	ivisurf->pending.prop = ivisurf->prop;
	wl_list_init(&ivisurf->pending.link);

	wl_list_init(&ivisurf->order.link);
	wl_list_init(&ivisurf->order.layer_list);

	wl_list_insert(&layout->surface_list, &ivisurf->link);

	wl_signal_emit(&layout->surface_notification.created, ivisurf);

	return ivisurf;
}

static void
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


static void
ivi_layout_surface_add_configured_listener(struct ivi_layout_surface* ivisurf,
					   struct wl_listener* listener)
{
	wl_signal_add(&ivisurf->configured, listener);
}

WL_EXPORT struct ivi_layout_interface ivi_layout_interface = {
	.get_weston_view = ivi_layout_get_weston_view,
	.surface_configure = ivi_layout_surface_configure,
	.surface_create = ivi_layout_surface_create,
	.init_with_compositor = ivi_layout_init_with_compositor,
	.get_surface_dimension = ivi_layout_surface_get_dimension,
	.add_surface_configured_listener = ivi_layout_surface_add_configured_listener
};

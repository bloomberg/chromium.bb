/*
 * Copyright (C) 2014 DENSO CORPORATION
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

#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "ivi-layout-export.h"
#include "ivi-layout-private.h"

struct ivi_layout_transition;

typedef void (*ivi_layout_transition_frame_func)(
			struct ivi_layout_transition *transition);
typedef void (*ivi_layout_transition_destroy_func)(
			struct ivi_layout_transition *transition);
typedef int32_t (*ivi_layout_is_transition_func)(void *private_data, void *id);

struct ivi_layout_transition {
	enum ivi_layout_transition_type type;
	void *private_data;
	void *user_data;

	uint32_t time_start;
	uint32_t time_duration;
	uint32_t time_elapsed;
	uint32_t  is_done;
	ivi_layout_is_transition_func is_transition_func;
	ivi_layout_transition_frame_func frame_func;
	ivi_layout_transition_destroy_func destroy_func;
};

struct transition_node {
	struct ivi_layout_transition *transition;
	struct wl_list link;
};

static void layout_transition_destroy(struct ivi_layout_transition *transition);

static struct ivi_layout_transition *
get_transition_from_type_and_id(enum ivi_layout_transition_type type,
				void *id_data)
{
	struct ivi_layout *layout = get_instance();
	struct transition_node *node;
	struct ivi_layout_transition *tran;

	wl_list_for_each(node, &layout->transitions->transition_list, link) {
		tran = node->transition;

		if (tran->type == type &&
		    tran->is_transition_func(tran->private_data, id_data))
			return tran;
	}

	return NULL;
}

WL_EXPORT int32_t
is_surface_transition(struct ivi_layout_surface *surface)
{
	struct ivi_layout *layout = get_instance();
	struct transition_node *node;
	struct ivi_layout_transition *tran;

	wl_list_for_each(node, &layout->transitions->transition_list, link) {
		tran = node->transition;

		if ((tran->type == IVI_LAYOUT_TRANSITION_VIEW_MOVE_RESIZE ||
		     tran->type == IVI_LAYOUT_TRANSITION_VIEW_RESIZE) &&
		    tran->is_transition_func(tran->private_data, surface))
			return 1;
	}

	return 0;
}

static void
tick_transition(struct ivi_layout_transition *transition, uint32_t timestamp)
{
	const double t = timestamp - transition->time_start;

	if (transition->time_duration <= t) {
		transition->time_elapsed = transition->time_duration;
		transition->is_done = 1;
	} else {
		transition->time_elapsed = t;
	}
}

static float time_to_nowpos(struct ivi_layout_transition *transition)
{
	return sin((float)transition->time_elapsed /
		   (float)transition->time_duration * M_PI_2);
}

static void
do_transition_frame(struct ivi_layout_transition *transition,
		    uint32_t timestamp)
{
	if (0 == transition->time_start)
		transition->time_start = timestamp;

	tick_transition(transition, timestamp);
	transition->frame_func(transition);

	if (transition->is_done)
		layout_transition_destroy(transition);
}

static int32_t
layout_transition_frame(void *data)
{
	struct ivi_layout_transition_set *transitions = data;
	uint32_t fps = 30;
	struct timespec timestamp = {};
	uint32_t msec = 0;
	struct transition_node *node = NULL;
	struct transition_node *next = NULL;

	if (wl_list_empty(&transitions->transition_list)) {
		wl_event_source_timer_update(transitions->event_source, 0);
		return 1;
	}

	wl_event_source_timer_update(transitions->event_source, 1000 / fps);

	clock_gettime(CLOCK_MONOTONIC, &timestamp);/* FIXME */
	msec = (1e+3 * timestamp.tv_sec + 1e-6 * timestamp.tv_nsec);

	wl_list_for_each_safe(node, next, &transitions->transition_list, link) {
		do_transition_frame(node->transition, msec);
	}

	ivi_layout_commit_changes();
	return 1;
}

WL_EXPORT struct ivi_layout_transition_set *
ivi_layout_transition_set_create(struct weston_compositor *ec)
{
	struct ivi_layout_transition_set *transitions;
	struct wl_event_loop *loop;

	transitions = malloc(sizeof(*transitions));
	if (transitions == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return NULL;
	}

	wl_list_init(&transitions->transition_list);

	loop = wl_display_get_event_loop(ec->wl_display);
	transitions->event_source =
		wl_event_loop_add_timer(loop, layout_transition_frame,
					transitions);

	return transitions;
}

static void
layout_transition_register(struct ivi_layout_transition *trans)
{
	struct ivi_layout *layout = get_instance();
	struct transition_node *node;

	node = malloc(sizeof(*node));
	if (node == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	node->transition = trans;
	wl_list_insert(&layout->pending_transition_list, &node->link);
}

static void
remove_transition(struct ivi_layout *layout,
		  struct ivi_layout_transition *trans)
{
	struct transition_node *node;
	struct transition_node *next;

	wl_list_for_each_safe(node, next,
			      &layout->transitions->transition_list, link) {
		if (node->transition == trans) {
			wl_list_remove(&node->link);
			free(node);
			return;
		}
	}

	wl_list_for_each_safe(node, next,
			      &layout->pending_transition_list, link) {
		if (node->transition == trans) {
			wl_list_remove(&node->link);
			free(node);
			return;
		}
	}
}

static void
layout_transition_destroy(struct ivi_layout_transition *transition)
{
	struct ivi_layout *layout = get_instance();

	remove_transition(layout, transition);
	if(transition->destroy_func)
		transition->destroy_func(transition);
	free(transition);
}

static struct ivi_layout_transition *
create_layout_transition(void)
{
	struct ivi_layout_transition *transition = malloc(sizeof(*transition));

	if (transition == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return NULL;
	}

	transition->type = IVI_LAYOUT_TRANSITION_MAX;
	transition->time_start = 0;
	transition->time_duration = 300; /* 300ms */
	transition->time_elapsed = 0;

	transition->is_done = 0;

	transition->private_data = NULL;
	transition->user_data = NULL;

	transition->frame_func = NULL;
	transition->destroy_func = NULL;

	return transition;
}

/* move and resize view transition */

struct move_resize_view_data {
	struct ivi_layout_surface *surface;
	int32_t start_x;
	int32_t start_y;
	int32_t end_x;
	int32_t end_y;
	int32_t start_width;
	int32_t start_height;
	int32_t end_width;
	int32_t end_height;
};

static void
transition_move_resize_view_destroy(struct ivi_layout_transition *transition)
{
	struct move_resize_view_data *data =
		(struct move_resize_view_data *)transition->private_data;
	struct ivi_layout_surface *layout_surface = data->surface;

	wl_signal_emit(&layout_surface->configured, layout_surface);

	if (transition->private_data) {
		free(transition->private_data);
		transition->private_data = NULL;
	}
}

static void
transition_move_resize_view_user_frame(struct ivi_layout_transition *transition)
{
	struct move_resize_view_data *mrv = transition->private_data;
	const double current = time_to_nowpos(transition);

	const int32_t destx = mrv->start_x +
		(mrv->end_x - mrv->start_x) * current;

	const int32_t desty = mrv->start_y +
		(mrv->end_y - mrv->start_y) * current;

	const int32_t dest_width = mrv->start_width  +
		(mrv->end_width - mrv->start_width) * current;

	const int32_t dest_height = mrv->start_height +
		(mrv->end_height - mrv->start_height) * current;

	ivi_layout_surface_set_destination_rectangle(mrv->surface,
						     destx, desty,
						     dest_width, dest_height);
}

static int32_t
is_transition_move_resize_view_func(struct move_resize_view_data *data,
				    struct ivi_layout_surface *view)
{
	return data->surface == view;
}

static struct ivi_layout_transition *
create_move_resize_view_transition(
			struct ivi_layout_surface *surface,
			int32_t start_x, int32_t start_y,
			int32_t end_x, int32_t end_y,
			int32_t start_width, int32_t start_height,
			int32_t end_width, int32_t end_height,
			ivi_layout_transition_frame_func frame_func,
			ivi_layout_transition_destroy_func destroy_func,
			uint32_t duration)
{
	struct ivi_layout_transition *transition = create_layout_transition();
	struct move_resize_view_data *data = malloc(sizeof(*data));

	if (data == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return NULL;
	}

	transition->type = IVI_LAYOUT_TRANSITION_VIEW_MOVE_RESIZE;
	transition->is_transition_func = (ivi_layout_is_transition_func)is_transition_move_resize_view_func;

	transition->frame_func = frame_func;
	transition->destroy_func = destroy_func;
	transition->private_data = data;

	if (duration != 0)
		transition->time_duration = duration;

	data->surface = surface;
	data->start_x = start_x;
	data->start_y = start_y;
	data->end_x   = end_x;
	data->end_y   = end_y;

	data->start_width  = start_width;
	data->start_height = start_height;
	data->end_width    = end_width;
	data->end_height   = end_height;

	return transition;
}

WL_EXPORT void
ivi_layout_transition_move_resize_view(struct ivi_layout_surface *surface,
				       int32_t dest_x, int32_t dest_y,
				       int32_t dest_width, int32_t dest_height,
				       uint32_t duration)
{
	struct ivi_layout_transition *transition;
	int32_t start_pos[2] = {
		surface->pending.prop.start_x,
		surface->pending.prop.start_y
	};

	int32_t start_size[2] = {
		surface->pending.prop.start_width,
		surface->pending.prop.start_height
	};

	transition = get_transition_from_type_and_id(
					IVI_LAYOUT_TRANSITION_VIEW_MOVE_RESIZE,
					surface);
	if (transition) {
		struct move_resize_view_data *data = transition->private_data;
		transition->time_start = 0;
		transition->time_duration = duration;

		data->start_x = start_pos[0];
		data->start_y = start_pos[1];
		data->end_x   = dest_x;
		data->end_y   = dest_y;

		data->start_width  = start_size[0];
		data->start_height = start_size[1];
		data->end_width    = dest_width;
		data->end_height   = dest_height;
		return;
	}

	transition = create_move_resize_view_transition(
		surface,
		start_pos[0], start_pos[1],
		dest_x, dest_y,
		start_size[0], start_size[1],
		dest_width, dest_height,
		transition_move_resize_view_user_frame,
		transition_move_resize_view_destroy,
		duration);

	layout_transition_register(transition);
}

/* fade transition */
struct fade_view_data {
	struct ivi_layout_surface *surface;
	double start_alpha;
	double end_alpha;
};

struct store_alpha{
	double alpha;
};

static void
fade_view_user_frame(struct ivi_layout_transition *transition)
{
	struct fade_view_data *fade = transition->private_data;
	struct ivi_layout_surface *surface = fade->surface;

	const double current = time_to_nowpos(transition);
	const double alpha = fade->start_alpha +
		(fade->end_alpha - fade->start_alpha) * current;

	ivi_layout_surface_set_opacity(surface, wl_fixed_from_double(alpha));
	ivi_layout_surface_set_visibility(surface, true);
}

static int32_t
is_transition_fade_view_func(struct fade_view_data *data,
			     struct ivi_layout_surface *view)
{
	return data->surface == view;
}

static struct ivi_layout_transition *
create_fade_view_transition(
			struct ivi_layout_surface *surface,
			double start_alpha, double end_alpha,
			ivi_layout_transition_frame_func frame_func,
			void *user_data,
			ivi_layout_transition_destroy_func destroy_func,
			uint32_t duration)
{
	struct ivi_layout_transition *transition = create_layout_transition();
	struct fade_view_data *data = malloc(sizeof(*data));

	if (data == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return NULL;
	}

	transition->type = IVI_LAYOUT_TRANSITION_VIEW_FADE;
	transition->is_transition_func = (ivi_layout_is_transition_func)is_transition_fade_view_func;

	transition->user_data = user_data;
	transition->private_data = data;
	transition->frame_func = frame_func;
	transition->destroy_func = destroy_func;

	if (duration != 0)
		transition->time_duration = duration;

	data->surface = surface;
	data->start_alpha = start_alpha;
	data->end_alpha   = end_alpha;

	return transition;
}

static void
create_visibility_transition(struct ivi_layout_surface *surface,
			     double start_alpha,
			     double dest_alpha,
			     void *user_data,
			     ivi_layout_transition_destroy_func destroy_func,
			     uint32_t duration)
{
	struct ivi_layout_transition *transition = NULL;

	transition = create_fade_view_transition(
		surface,
		start_alpha, dest_alpha,
		fade_view_user_frame,
		user_data,
		destroy_func,
		duration);

	layout_transition_register(transition);
}

static void
visibility_on_transition_destroy(struct ivi_layout_transition *transition)
{
	struct fade_view_data *data = transition->private_data;
	struct store_alpha *user_data = transition->user_data;

	ivi_layout_surface_set_visibility(data->surface, true);

	free(data);
	transition->private_data = NULL;

	free(user_data);
	transition->user_data = NULL;
}

WL_EXPORT void
ivi_layout_transition_visibility_on(struct ivi_layout_surface *surface,
				    uint32_t duration)
{
	struct ivi_layout_transition *transition;
	bool is_visible = ivi_layout_surface_get_visibility(surface);
	wl_fixed_t dest_alpha = ivi_layout_surface_get_opacity(surface);
	struct store_alpha *user_data = NULL;
	wl_fixed_t start_alpha = 0.0;
	struct fade_view_data *data = NULL;

	transition = get_transition_from_type_and_id(
					IVI_LAYOUT_TRANSITION_VIEW_FADE,
					surface);
	if (transition) {
		start_alpha = ivi_layout_surface_get_opacity(surface);
		user_data = transition->user_data;
		data = transition->private_data;

		transition->time_start = 0;
		transition->time_duration = duration;
		transition->destroy_func = visibility_on_transition_destroy;

		data->start_alpha = wl_fixed_to_double(start_alpha);
		data->end_alpha = user_data->alpha;
		return;
	}

	if (is_visible)
		return;

	user_data = malloc(sizeof(*user_data));
	if (user_data == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	user_data->alpha = wl_fixed_to_double(dest_alpha);

	create_visibility_transition(surface,
				     0.0, // start_alpha
				     wl_fixed_to_double(dest_alpha),
				     user_data,
				     visibility_on_transition_destroy,
				     duration);
}

static void
visibility_off_transition_destroy(struct ivi_layout_transition *transition)
{
	struct fade_view_data *data = transition->private_data;
	struct store_alpha *user_data = transition->user_data;

	ivi_layout_surface_set_visibility(data->surface, false);

	ivi_layout_surface_set_opacity(data->surface,
				       wl_fixed_from_double(user_data->alpha));

	free(data);
	transition->private_data = NULL;

	free(user_data);
	transition->user_data= NULL;
}

WL_EXPORT void
ivi_layout_transition_visibility_off(struct ivi_layout_surface *surface,
				     uint32_t duration)
{
	struct ivi_layout_transition *transition;
	wl_fixed_t start_alpha = ivi_layout_surface_get_opacity(surface);
	struct store_alpha* user_data = NULL;
	struct fade_view_data* data = NULL;

	transition =
		get_transition_from_type_and_id(IVI_LAYOUT_TRANSITION_VIEW_FADE,
						surface);
	if (transition) {
		data = transition->private_data;

		transition->time_start = 0;
		transition->time_duration = duration;
		transition->destroy_func = visibility_off_transition_destroy;

		data->start_alpha = wl_fixed_to_double(start_alpha);
		data->end_alpha = 0;
		return;
	}

	user_data = malloc(sizeof(*user_data));
	if (user_data == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	user_data->alpha = wl_fixed_to_double(start_alpha);

	create_visibility_transition(surface,
				     wl_fixed_to_double(start_alpha),
				     0.0, // dest_alpha
				     user_data,
				     visibility_off_transition_destroy,
				     duration);
}

/* move layer transition */

struct move_layer_data {
	struct ivi_layout_layer *layer;
	int32_t start_x;
	int32_t start_y;
	int32_t end_x;
	int32_t end_y;
	ivi_layout_transition_destroy_user_func destroy_func;
};

static void
transition_move_layer_user_frame(struct ivi_layout_transition *transition)
{
	struct move_layer_data *data = transition->private_data;
	struct ivi_layout_layer *layer = data->layer;

	const float  current = time_to_nowpos(transition);

	const int32_t dest_x = data->start_x +
		(data->end_x - data->start_x) * current;

	const int32_t dest_y = data->start_y +
		(data->end_y - data->start_y) * current;

	ivi_layout_layer_set_position(layer, dest_x, dest_y);
}

static void
transition_move_layer_destroy(struct ivi_layout_transition *transition)
{
	struct move_layer_data *data = transition->private_data;

	if(data->destroy_func)
		data->destroy_func(transition->user_data);

	free(data);
	transition->private_data = NULL;
}

static int32_t
is_transition_move_layer_func(struct move_layer_data *data,
			      struct ivi_layout_layer *layer)
{
	return data->layer == layer;
}


static struct ivi_layout_transition *
create_move_layer_transition(
		struct ivi_layout_layer *layer,
		int32_t start_x, int32_t start_y,
		int32_t end_x, int32_t end_y,
		void *user_data,
		ivi_layout_transition_destroy_user_func destroy_user_func,
		uint32_t duration)
{
	struct ivi_layout_transition *transition = create_layout_transition();
	struct move_layer_data *data = malloc(sizeof(*data));

	if (data == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return NULL;
	}

	transition->type = IVI_LAYOUT_TRANSITION_LAYER_MOVE;
	transition->is_transition_func = (ivi_layout_is_transition_func)is_transition_move_layer_func;

	transition->frame_func = transition_move_layer_user_frame;
	transition->destroy_func = transition_move_layer_destroy;
	transition->private_data = data;
	transition->user_data = user_data;

	if (duration != 0)
		transition->time_duration = duration;

	data->layer = layer;
	data->start_x = start_x;
	data->start_y = start_y;
	data->end_x   = end_x;
	data->end_y   = end_y;
	data->destroy_func = destroy_user_func;

	return transition;
}

WL_EXPORT void
ivi_layout_transition_move_layer(struct ivi_layout_layer *layer,
				 int32_t dest_x, int32_t dest_y,
				 uint32_t duration)
{
	int32_t start_pos_x = 0;
	int32_t start_pos_y = 0;
	struct ivi_layout_transition *transition = NULL;

	ivi_layout_layer_get_position(layer, &start_pos_x, &start_pos_y);

	transition = create_move_layer_transition(
		layer,
		start_pos_x, start_pos_y,
		dest_x, dest_y,
		NULL, NULL,
		duration);

	layout_transition_register(transition);

	return;
}

WL_EXPORT void
ivi_layout_transition_move_layer_cancel(struct ivi_layout_layer *layer)
{
	struct ivi_layout_transition *transition =
		get_transition_from_type_and_id(
					IVI_LAYOUT_TRANSITION_LAYER_MOVE,
					layer);
	if (transition) {
		layout_transition_destroy(transition);
	}
}

/* fade layer transition */
struct fade_layer_data {
	struct ivi_layout_layer *layer;
	uint32_t is_fade_in;
	double start_alpha;
	double end_alpha;
	ivi_layout_transition_destroy_user_func destroy_func;
};

static void
transition_fade_layer_destroy(struct ivi_layout_transition *transition)
{
	struct fade_layer_data *data = transition->private_data;
	transition->private_data = NULL;

	free(data);
}

static void
transition_fade_layer_user_frame(struct ivi_layout_transition *transition)
{
	double current = time_to_nowpos(transition);
	struct fade_layer_data *data = transition->private_data;
	double alpha = data->start_alpha +
		(data->end_alpha - data->start_alpha) * current;
	wl_fixed_t fixed_alpha = wl_fixed_from_double(alpha);

	int32_t is_done = transition->is_done;
	bool is_visible = !is_done || data->is_fade_in;

	ivi_layout_layer_set_opacity(data->layer, fixed_alpha);
	ivi_layout_layer_set_visibility(data->layer, is_visible);
}

static int32_t
is_transition_fade_layer_func(struct fade_layer_data *data,
			      struct ivi_layout_layer *layer)
{
	return data->layer == layer;
}

WL_EXPORT void
ivi_layout_transition_fade_layer(
			struct ivi_layout_layer *layer,
			uint32_t is_fade_in,
			double start_alpha, double end_alpha,
			void* user_data,
			ivi_layout_transition_destroy_user_func destroy_func,
			uint32_t duration)
{
	struct ivi_layout_transition *transition;
	struct fade_layer_data *data = NULL;
	wl_fixed_t fixed_opacity = 0.0;
	double now_opacity = 0.0;
	double remain = 0.0;

	transition = get_transition_from_type_and_id(
					IVI_LAYOUT_TRANSITION_LAYER_FADE,
					layer);
	if (transition) {
		/* transition update */
		data = transition->private_data;

		/* FIXME */
		fixed_opacity = ivi_layout_layer_get_opacity(layer);
		now_opacity = wl_fixed_to_double(fixed_opacity);
		remain = 0.0;

		data->is_fade_in = is_fade_in;
		data->start_alpha = now_opacity;
		data->end_alpha = end_alpha;

		remain = is_fade_in? 1.0 - now_opacity : now_opacity;
		transition->time_start = 0;
		transition->time_elapsed = 0;
		transition->time_duration = duration * remain;

		return;
	}

	transition = create_layout_transition();
	data = malloc(sizeof(*data));

	if (data == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	transition->type = IVI_LAYOUT_TRANSITION_LAYER_FADE;
	transition->is_transition_func = (ivi_layout_is_transition_func)is_transition_fade_layer_func;

	transition->private_data = data;
	transition->user_data = user_data;

	transition->frame_func = transition_fade_layer_user_frame;
	transition->destroy_func = transition_fade_layer_destroy;

	if (duration != 0)
		transition->time_duration = duration;

	data->layer = layer;
	data->is_fade_in = is_fade_in;
	data->start_alpha = start_alpha;
	data->end_alpha = end_alpha;
	data->destroy_func = destroy_func;

	layout_transition_register(transition);

	return;
}

/* render order transition */
struct surface_reorder {
	uint32_t id_surface;
	uint32_t new_index;
};

struct change_order_data {
	struct ivi_layout_layer *layer;
	uint32_t surface_num;
	struct surface_reorder *reorder;
};

struct surf_with_index {
	uint32_t id_surface;
	float surface_index;
};

static int
cmp_order_asc(const void *lhs, const void *rhs)
{
	const struct surf_with_index *l = lhs;
	const struct surf_with_index *r = rhs;

	return l->surface_index > r->surface_index;
}

/*
render oerder transition

index   0      1      2
old   surfA, surfB, surfC
new   surfB, surfC, surfA
       (-1)  (-1)   (+2)

after 10% of time elapsed
       0.2    0.9    1.9
      surfA, surfB, surfC

after 50% of time elapsed
       0.5    1.0    1.5
      surfB, surfA, surfC
*/

static void
transition_change_order_user_frame(struct ivi_layout_transition *transition)
{
	uint32_t i, old_index;
	double current = time_to_nowpos(transition);
	struct change_order_data *data = transition->private_data;

	struct surf_with_index *swi = malloc(sizeof(*swi) * data->surface_num);
	struct ivi_layout_surface **new_surface_order = NULL;
	uint32_t surface_num = 0;

	if (swi == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	for (old_index = 0; old_index < data->surface_num; old_index++) {
		swi[old_index].id_surface = data->reorder[old_index].id_surface;
		swi[old_index].surface_index = (float)old_index +
			((float)data->reorder[old_index].new_index - (float)old_index) * current;
	}

	qsort(swi, data->surface_num, sizeof(*swi), cmp_order_asc);

	new_surface_order =
		malloc(sizeof(*new_surface_order) * data->surface_num);

	if (new_surface_order == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	for (i = 0; i < data->surface_num; i++) {
		struct ivi_layout_surface *surf =
			ivi_layout_get_surface_from_id(swi[i].id_surface);
		if(surf)
			new_surface_order[surface_num++] = surf;
	}

	ivi_layout_layer_set_render_order(data->layer, new_surface_order,
					  surface_num);

	free(new_surface_order);
	free(swi);
}

static void
transition_change_order_destroy(struct ivi_layout_transition *transition)
{
	struct change_order_data *data = transition->private_data;

	free(data->reorder);
	free(data);
}

static int32_t find_surface(struct ivi_layout_surface **surfaces,
			    uint32_t surface_num,
			    struct ivi_layout_surface *target)
{
	uint32_t i = 0;

	for(i = 0; i < surface_num; i++) {
		if (surfaces[i] == target)
			return i;
	}

	return -1;
}

static int32_t
is_transition_change_order_func(struct change_order_data *data,
				struct ivi_layout_layer *layer)
{
	return data->layer == layer;
}

WL_EXPORT void
ivi_layout_transition_layer_render_order(struct ivi_layout_layer *layer,
					 struct ivi_layout_surface **new_order,
					 uint32_t surface_num,
					 uint32_t duration)
{
	struct surface_reorder *reorder;
	struct ivi_layout_surface *surf;
	uint32_t old_index = 0;
	struct ivi_layout_transition *transition;
	struct change_order_data *data = NULL;
	int32_t new_index = 0;
	uint32_t id = 0;

	reorder = malloc(sizeof(*reorder) * surface_num);
	if (reorder == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	wl_list_for_each(surf, &layer->order.surface_list, order.link) {
		new_index = find_surface(new_order, surface_num, surf);
		id = ivi_layout_get_id_of_surface(surf);
		if(new_index < 0){
			fprintf(stderr, "invalid render order!!!\n");
			return;
		}

		reorder[old_index].id_surface = id;
		reorder[old_index].new_index = new_index;
		old_index++;
	}

	transition = get_transition_from_type_and_id(
					IVI_LAYOUT_TRANSITION_LAYER_VIEW_ORDER,
					layer);
	if (transition) {
		/* update transition */
		struct change_order_data *data = transition->private_data;
		transition->time_start = 0; /* timer reset */

		if (duration != 0) {
			transition->time_duration = duration;
		}

		free(data->reorder);
		data->reorder = reorder;
		return;
	}

	transition = create_layout_transition();
	data = malloc(sizeof(*data));

	if (data == NULL) {
		weston_log("%s: memory allocation fails\n", __func__);
		return;
	}

	transition->type = IVI_LAYOUT_TRANSITION_LAYER_VIEW_ORDER;
	transition->is_transition_func = (ivi_layout_is_transition_func)is_transition_change_order_func;

	transition->private_data = data;
	transition->frame_func = transition_change_order_user_frame;
	transition->destroy_func = transition_change_order_destroy;

	if (duration != 0)
		transition->time_duration = duration;

	data->layer = layer;
	data->reorder = reorder;
	data->surface_num = old_index;

	layout_transition_register(transition);
}

WL_EXPORT int32_t
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

int32_t
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

WL_EXPORT int32_t
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

WL_EXPORT int32_t
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

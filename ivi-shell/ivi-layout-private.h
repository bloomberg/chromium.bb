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

#ifndef _ivi_layout_PRIVATE_H_
#define _ivi_layout_PRIVATE_H_

#include "compositor.h"
#include "ivi-layout-export.h"

struct ivi_layout_surface {
	struct wl_list link;
	struct wl_signal property_changed;
	struct wl_list layer_list;
	int32_t update_count;
	uint32_t id_surface;

	struct ivi_layout *layout;
	struct weston_surface *surface;

	struct wl_listener surface_destroy_listener;
	struct weston_transform surface_rotation;
	struct weston_transform layer_rotation;
	struct weston_transform surface_pos;
	struct weston_transform layer_pos;
	struct weston_transform scaling;

	struct ivi_layout_surface_properties prop;
	uint32_t event_mask;

	struct {
		struct ivi_layout_surface_properties prop;
		struct wl_list link;
	} pending;

	struct {
		struct wl_list link;
		struct wl_list layer_list;
	} order;

	struct {
		ivi_controller_surface_content_callback callback;
		void *userdata;
	} content_observer;

	struct wl_signal configured;
};

struct ivi_layout_layer {
	struct wl_list link;
	struct wl_signal property_changed;
	struct wl_list screen_list;
	struct wl_list link_to_surface;
	uint32_t id_layer;

	struct ivi_layout *layout;

	struct ivi_layout_layer_properties prop;
	uint32_t event_mask;

	struct {
		struct ivi_layout_layer_properties prop;
		struct wl_list surface_list;
		struct wl_list link;
	} pending;

	struct {
		struct wl_list surface_list;
		struct wl_list link;
	} order;
};

struct ivi_layout {
	struct weston_compositor *compositor;

	struct wl_list surface_list;
	struct wl_list layer_list;
	struct wl_list screen_list;

	struct {
		struct wl_signal created;
		struct wl_signal removed;
	} layer_notification;

	struct {
		struct wl_signal created;
		struct wl_signal removed;
		struct wl_signal configure_changed;
	} surface_notification;

	struct weston_layer layout_layer;
	struct wl_signal warning_signal;

	struct ivi_layout_transition_set *transitions;
	struct wl_list pending_transition_list;
};

struct ivi_layout *get_instance(void);

struct ivi_layout_transition;

struct ivi_layout_transition_set {
	struct wl_event_source  *event_source;
	struct wl_list          transition_list;
};

typedef void (*ivi_layout_transition_destroy_user_func)(void *user_data);

struct ivi_layout_transition_set *
ivi_layout_transition_set_create(struct weston_compositor *ec);

void
ivi_layout_transition_move_resize_view(struct ivi_layout_surface *surface,
				       int32_t dest_x, int32_t dest_y,
				       int32_t dest_width, int32_t dest_height,
				       uint32_t duration);

void
ivi_layout_transition_visibility_on(struct ivi_layout_surface *surface,
				    uint32_t duration);

void
ivi_layout_transition_visibility_off(struct ivi_layout_surface *surface,
				     uint32_t duration);


void
ivi_layout_transition_move_layer(struct ivi_layout_layer *layer,
				 int32_t dest_x, int32_t dest_y,
				 uint32_t duration);

void
ivi_layout_transition_fade_layer(struct ivi_layout_layer *layer,
				 uint32_t is_fade_in,
				 double start_alpha, double end_alpha,
				 void *user_data,
				 ivi_layout_transition_destroy_user_func destroy_func,
				 uint32_t duration);

int32_t
is_surface_transition(struct ivi_layout_surface *surface);

/**
 * \brief get ivi_layout_layer from id of layer
 *
 * \return (struct ivi_layout_layer *)
 *              if the method call was successful
 * \return NULL if the method call was failed
 */
struct ivi_layout_layer *
ivi_layout_get_layer_from_id(uint32_t id_layer);

/**
 * \brief Remove a surface
 */
void
ivi_layout_surface_remove(struct ivi_layout_surface *ivisurf);

/**
 * \brief Get all Layers of the given screen
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_layers_on_screen(struct ivi_layout_screen *iviscrn,
				int32_t *pLength,
				struct ivi_layout_layer ***ppArray);

/**
 * \brief Get all Surfaces which are currently registered to a given
 * layer and are managed by the services
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_surfaces_on_layer(struct ivi_layout_layer *ivilayer,
				 int32_t *pLength,
				 struct ivi_layout_surface ***ppArray);

/**
 * \brief Get the visibility of a layer. If a layer is not visible,
 * the layer and its surfaces will not be rendered.
 *
 * \return true if layer is visible
 * \return false if layer is invisible or the method call was failed
 */
bool
ivi_layout_layer_get_visibility(struct ivi_layout_layer *ivilayer);

/**
 * \brief Get the horizontal and vertical dimension of the layer.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_get_dimension(struct ivi_layout_layer *ivilayer,
			       int32_t *dest_width, int32_t *dest_height);

/**
 * \brief Set the horizontal and vertical dimension of the layer.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_dimension(struct ivi_layout_layer *ivilayer,
			       int32_t dest_width, int32_t dest_height);

/**
 * \brief Gets the orientation of a layer.
 *
 * \return (enum wl_output_transform)
 *              if the method call was successful
 * \return WL_OUTPUT_TRANSFORM_NORMAL if the method call was failed
 */
enum wl_output_transform
ivi_layout_layer_get_orientation(struct ivi_layout_layer *ivilayer);

/**
 * \brief Set the horizontal and vertical dimension of the surface.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_dimension(struct ivi_layout_surface *ivisurf,
				 int32_t dest_width, int32_t dest_height);

/**
 * \brief Get the horizontal and vertical dimension of the surface.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_get_dimension(struct ivi_layout_surface *ivisurf,
				 int32_t *dest_width, int32_t *dest_height);

/**
 * \brief Sets the horizontal and vertical position of the surface.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_position(struct ivi_layout_surface *ivisurf,
				int32_t dest_x, int32_t dest_y);

/**
 * \brief Get the horizontal and vertical position of the surface.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_get_position(struct ivi_layout_surface *ivisurf,
				int32_t *dest_x, int32_t *dest_y);

/**
 * \brief Gets the orientation of a surface.
 *
 * \return (enum wl_output_transform)
 *              if the method call was successful
 * \return WL_OUTPUT_TRANSFORM_NORMAL if the method call was failed
 */
enum wl_output_transform
ivi_layout_surface_get_orientation(struct ivi_layout_surface *ivisurf);

int32_t
ivi_layout_surface_set_transition_duration(
			struct ivi_layout_surface *ivisurf,
			uint32_t duration);

struct ivi_layout_interface {
	struct weston_view *(*get_weston_view)(
				struct ivi_layout_surface *surface);

	void (*surface_configure)(struct ivi_layout_surface *ivisurf,
				  int32_t width,
				  int32_t height);

	struct ivi_layout_surface *(*surface_create)(
				struct weston_surface *wl_surface,
				uint32_t id_surface);

	void (*init_with_compositor)(struct weston_compositor *ec);

	int32_t (*get_surface_dimension)(
				struct ivi_layout_surface *ivisurf,
				int32_t *dest_width,
				int32_t *dest_height);

	void (*add_surface_configured_listener)(
				struct ivi_layout_surface *ivisurf,
				struct wl_listener* listener);
};

#endif

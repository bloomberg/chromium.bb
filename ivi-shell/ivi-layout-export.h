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
 * The ivi-layout library supports API set of controlling properties of
 * surface and layer which groups surfaces. An unique ID whose type is integer
 * is required to create surface and layer. With the unique ID, surface and
 * layer are identified to control them. The API set consists of APIs to control
 * properties of surface and layers about followings,
 * - visibility.
 * - opacity.
 * - clipping (x,y,width,height).
 * - position and size of it to be displayed.
 * - orientation per 90 degree.
 * - add or remove surfaces to a layer.
 * - order of surfaces/layers in layer/screen to be displayed.
 * - commit to apply property changes.
 * - notifications of property change.
 *
 * Management of surfaces and layers grouping these surfaces are common
 * way in In-Vehicle Infotainment system, which integrate several domains
 * in one system. A layer is allocated to a domain in order to control
 * application surfaces grouped to the layer all together.
 *
 * This API and ABI follow following specifications.
 * http://projects.genivi.org/wayland-ivi-extension/layer-manager-apis
 */

#ifndef _IVI_LAYOUT_EXPORT_H_
#define _IVI_LAYOUT_EXPORT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "stdbool.h"
#include "compositor.h"

#define IVI_SUCCEEDED (0)
#define IVI_FAILED (-1)

struct ivi_layout_layer;
struct ivi_layout_screen;
struct ivi_layout_surface;

struct ivi_layout_surface_properties
{
	wl_fixed_t opacity;
	int32_t source_x;
	int32_t source_y;
	int32_t source_width;
	int32_t source_height;
	int32_t start_x;
	int32_t start_y;
	int32_t start_width;
	int32_t start_height;
	int32_t dest_x;
	int32_t dest_y;
	int32_t dest_width;
	int32_t dest_height;
	enum wl_output_transform orientation;
	bool visibility;
	int32_t transition_type;
	uint32_t transition_duration;
};

struct ivi_layout_layer_properties
{
	wl_fixed_t opacity;
	int32_t source_x;
	int32_t source_y;
	int32_t source_width;
	int32_t source_height;
	int32_t dest_x;
	int32_t dest_y;
	int32_t dest_width;
	int32_t dest_height;
	enum wl_output_transform orientation;
	uint32_t visibility;
	int32_t transition_type;
	uint32_t transition_duration;
	double start_alpha;
	double end_alpha;
	uint32_t is_fade_in;
};

enum ivi_layout_notification_mask {
	IVI_NOTIFICATION_NONE        = 0,
	IVI_NOTIFICATION_OPACITY     = (1 << 1),
	IVI_NOTIFICATION_SOURCE_RECT = (1 << 2),
	IVI_NOTIFICATION_DEST_RECT   = (1 << 3),
	IVI_NOTIFICATION_DIMENSION   = (1 << 4),
	IVI_NOTIFICATION_POSITION    = (1 << 5),
	IVI_NOTIFICATION_ORIENTATION = (1 << 6),
	IVI_NOTIFICATION_VISIBILITY  = (1 << 7),
	IVI_NOTIFICATION_PIXELFORMAT = (1 << 8),
	IVI_NOTIFICATION_ADD         = (1 << 9),
	IVI_NOTIFICATION_REMOVE      = (1 << 10),
	IVI_NOTIFICATION_CONFIGURE   = (1 << 11),
	IVI_NOTIFICATION_ALL         = 0xFFFF
};

enum ivi_layout_transition_type{
	IVI_LAYOUT_TRANSITION_NONE,
	IVI_LAYOUT_TRANSITION_VIEW_DEFAULT,
	IVI_LAYOUT_TRANSITION_VIEW_DEST_RECT_ONLY,
	IVI_LAYOUT_TRANSITION_VIEW_FADE_ONLY,
	IVI_LAYOUT_TRANSITION_LAYER_FADE,
	IVI_LAYOUT_TRANSITION_LAYER_MOVE,
	IVI_LAYOUT_TRANSITION_LAYER_VIEW_ORDER,
	IVI_LAYOUT_TRANSITION_VIEW_MOVE_RESIZE,
	IVI_LAYOUT_TRANSITION_VIEW_RESIZE,
	IVI_LAYOUT_TRANSITION_VIEW_FADE,
	IVI_LAYOUT_TRANSITION_MAX,
};

typedef void (*layer_property_notification_func)(
			struct ivi_layout_layer *ivilayer,
			const struct ivi_layout_layer_properties *,
			enum ivi_layout_notification_mask mask,
			void *userdata);

typedef void (*surface_property_notification_func)(
			struct ivi_layout_surface *ivisurf,
			const struct ivi_layout_surface_properties *,
			enum ivi_layout_notification_mask mask,
			void *userdata);

typedef void (*layer_create_notification_func)(
			struct ivi_layout_layer *ivilayer,
			void *userdata);

typedef void (*layer_remove_notification_func)(
			struct ivi_layout_layer *ivilayer,
			void *userdata);

typedef void (*surface_create_notification_func)(
			struct ivi_layout_surface *ivisurf,
			void *userdata);

typedef void (*surface_remove_notification_func)(
			struct ivi_layout_surface *ivisurf,
			void *userdata);

typedef void (*surface_configure_notification_func)(
			struct ivi_layout_surface *ivisurf,
			void *userdata);

typedef void (*ivi_controller_surface_content_callback)(
			struct ivi_layout_surface *ivisurf,
			int32_t content,
			void *userdata);

/**
 * \brief register for notification when layer is created
 */
int32_t
ivi_layout_add_notification_create_layer(
			layer_create_notification_func callback,
			void *userdata);

void
ivi_layout_remove_notification_create_layer(
			layer_create_notification_func callback,
			void *userdata);

/**
 * \brief register for notification when layer is removed
 */
int32_t
ivi_layout_add_notification_remove_layer(
			layer_remove_notification_func callback,
			void *userdata);

void
ivi_layout_remove_notification_remove_layer(
			layer_remove_notification_func callback,
			void *userdata);

/**
 * \brief register for notification when surface is created
 */
int32_t
ivi_layout_add_notification_create_surface(
			surface_create_notification_func callback,
			void *userdata);

void
ivi_layout_remove_notification_create_surface(
			surface_create_notification_func callback,
			void *userdata);

/**
 * \brief register for notification when surface is removed
 */
int32_t
ivi_layout_add_notification_remove_surface(
			surface_remove_notification_func callback,
			void *userdata);

void
ivi_layout_remove_notification_remove_surface(
			surface_remove_notification_func callback,
			void *userdata);

/**
 * \brief register for notification when surface is configured
 */
int32_t
ivi_layout_add_notification_configure_surface(
			surface_configure_notification_func callback,
			void *userdata);

void
ivi_layout_remove_notification_configure_surface(
			surface_configure_notification_func callback,
			void *userdata);

/**
 * \brief get id of surface from ivi_layout_surface
 *
 * \return id of surface
 */
uint32_t
ivi_layout_get_id_of_surface(struct ivi_layout_surface *ivisurf);

/**
 * \brief get id of layer from ivi_layout_layer
 *
 *
 * \return id of layer
 */
uint32_t
ivi_layout_get_id_of_layer(struct ivi_layout_layer *ivilayer);

/**
 * \brief get ivi_layout_surface from id of surface
 *
 * \return (struct ivi_layout_surface *)
 *              if the method call was successful
 * \return NULL if the method call was failed
 */
struct ivi_layout_surface *
ivi_layout_get_surface_from_id(uint32_t id_surface);

/**
 * \brief get ivi_layout_screen from id of screen
 *
 * \return (struct ivi_layout_screen *)
 *              if the method call was successful
 * \return NULL if the method call was failed
 */
struct ivi_layout_screen *
ivi_layout_get_screen_from_id(uint32_t id_screen);

/**
 * \brief Get the screen resolution of a specific screen
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_screen_resolution(struct ivi_layout_screen *iviscrn,
				 int32_t *pWidth,
				 int32_t *pHeight);

/**
 * \brief Set an observer callback for surface content status change.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_content_observer(
			struct ivi_layout_surface *ivisurf,
			ivi_controller_surface_content_callback callback,
			void* userdata);

/**
 * \brief  Get the layer properties
 *
 * \return (const struct ivi_layout_layer_properties *)
 *              if the method call was successful
 * \return NULL if the method call was failed
 */
const struct ivi_layout_layer_properties *
ivi_layout_get_properties_of_layer(struct ivi_layout_layer *ivilayer);

/**
 * \brief Get the screens
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_screens(int32_t *pLength, struct ivi_layout_screen ***ppArray);

/**
 * \brief Get the screens under the given layer
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_screens_under_layer(struct ivi_layout_layer *ivilayer,
				   int32_t *pLength,
				   struct ivi_layout_screen ***ppArray);

/**
 * \brief Get all Layers which are currently registered and managed
 * by the services
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_layers(int32_t *pLength, struct ivi_layout_layer ***ppArray);

/**
 * \brief Get all Layers under the given surface
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_layers_under_surface(struct ivi_layout_surface *ivisurf,
				    int32_t *pLength,
				    struct ivi_layout_layer ***ppArray);

/**
 * \brief Get all Surfaces which are currently registered and managed
 * by the services
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_get_surfaces(int32_t *pLength, struct ivi_layout_surface ***ppArray);

/**
 * \brief Create a layer which should be managed by the service
 *
 * \return (struct ivi_layout_layer *)
 *              if the method call was successful
 * \return NULL if the method call was failed
 */
struct ivi_layout_layer *
ivi_layout_layer_create_with_dimension(uint32_t id_layer,
				       int32_t width, int32_t height);

/**
 * \brief Removes a layer which is currently managed by the service
 */
void
ivi_layout_layer_remove(struct ivi_layout_layer *ivilayer);

/**
 * \brief Set the visibility of a layer. If a layer is not visible, the
 * layer and its surfaces will not be rendered.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_visibility(struct ivi_layout_layer *ivilayer,
				bool newVisibility);

/**
 * \brief Set the opacity of a layer.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_opacity(struct ivi_layout_layer *ivilayer,
			     wl_fixed_t opacity);

/**
 * \brief Get the opacity of a layer.
 *
 * \return opacity if the method call was successful
 * \return wl_fixed_from_double(0.0) if the method call was failed
 */
wl_fixed_t
ivi_layout_layer_get_opacity(struct ivi_layout_layer *ivilayer);

/**
 * \brief Set the area of a layer which should be used for the rendering.
 *
 * Only this part will be visible.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_source_rectangle(struct ivi_layout_layer *ivilayer,
				      int32_t x, int32_t y,
				      int32_t width, int32_t height);

/**
 * \brief Set the destination area on the display for a layer.
 *
 * The layer will be scaled and positioned to this rectangle
 * for rendering
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_destination_rectangle(struct ivi_layout_layer *ivilayer,
					   int32_t x, int32_t y,
					   int32_t width, int32_t height);

/**
 * \brief Get the horizontal and vertical position of the layer.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_get_position(struct ivi_layout_layer *ivilayer,
			      int32_t *dest_x, int32_t *dest_y);

/**
 * \brief Sets the horizontal and vertical position of the layer.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_position(struct ivi_layout_layer *ivilayer,
			      int32_t dest_x, int32_t dest_y);

/**
 * \brief Sets the orientation of a layer.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_orientation(struct ivi_layout_layer *ivilayer,
				 enum wl_output_transform orientation);

/**
 * \brief Sets render order of surfaces within one layer
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_set_render_order(struct ivi_layout_layer *ivilayer,
				  struct ivi_layout_surface **pSurface,
				  int32_t number);

/**
 * \brief Set the visibility of a surface.
 *
 * If a surface is not visible it will not be rendered.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_visibility(struct ivi_layout_surface *ivisurf,
				  bool newVisibility);

/**
 * \brief Get the visibility of a surface.
 *
 * If a surface is not visible it will not be rendered.
 *
 * \return true if surface is visible
 * \return false if surface is invisible or the method call was failed
 */
bool
ivi_layout_surface_get_visibility(struct ivi_layout_surface *ivisurf);

/**
 * \brief Set the opacity of a surface.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_opacity(struct ivi_layout_surface *ivisurf,
			       wl_fixed_t opacity);

/**
 * \brief Get the opacity of a surface.
 *
 * \return opacity if the method call was successful
 * \return wl_fixed_from_double(0.0) if the method call was failed
 */
wl_fixed_t
ivi_layout_surface_get_opacity(struct ivi_layout_surface *ivisurf);

/**
 * \brief Set the destination area of a surface within a layer for rendering.
 *
 * The surface will be scaled to this rectangle for rendering.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_destination_rectangle(struct ivi_layout_surface *ivisurf,
					     int32_t x, int32_t y,
					     int32_t width, int32_t height);

/**
 * \brief Sets the orientation of a surface.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_orientation(struct ivi_layout_surface *ivisurf,
				   enum wl_output_transform orientation);

/**
 * \brief Add a layer to a screen which is currently managed by the service
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_screen_add_layer(struct ivi_layout_screen *iviscrn,
			    struct ivi_layout_layer *addlayer);

/**
 * \brief Sets render order of layers on a display
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_screen_set_render_order(struct ivi_layout_screen *iviscrn,
				   struct ivi_layout_layer **pLayer,
				   const int32_t number);

/**
 * \brief register for notification on property changes of layer
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_add_notification(struct ivi_layout_layer *ivilayer,
				  layer_property_notification_func callback,
				  void *userdata);

/**
 * \brief remove notification on property changes of layer
 */
void
ivi_layout_layer_remove_notification(struct ivi_layout_layer *ivilayer);

/**
 * \brief register for notification on property changes of surface
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_add_notification(struct ivi_layout_surface *ivisurf,
				    surface_property_notification_func callback,
				    void *userdata);

/**
 * \brief remove notification on property changes of surface
 */
void
ivi_layout_surface_remove_notification(struct ivi_layout_surface *ivisurf);

/**
 * \brief Get the surface properties
 *
 * \return (const struct ivi_surface_layer_properties *)
 *              if the method call was successful
 * \return NULL if the method call was failed
 */
const struct ivi_layout_surface_properties *
ivi_layout_get_properties_of_surface(struct ivi_layout_surface *ivisurf);

/**
 * \brief Add a surface to a layer which is currently managed by the service
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_layer_add_surface(struct ivi_layout_layer *ivilayer,
			     struct ivi_layout_surface *addsurf);

/**
 * \brief Removes a surface from a layer which is currently managed by the service
 */
void
ivi_layout_layer_remove_surface(struct ivi_layout_layer *ivilayer,
				struct ivi_layout_surface *remsurf);

/**
 * \brief Set the area of a surface which should be used for the rendering.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_surface_set_source_rectangle(struct ivi_layout_surface *ivisurf,
					int32_t x, int32_t y,
					int32_t width, int32_t height);

/**
 * \brief get weston_output from ivi_layout_screen.
 *
 * \return (struct weston_output *)
 *              if the method call was successful
 * \return NULL if the method call was failed
 */
struct weston_output *
ivi_layout_screen_get_output(struct ivi_layout_screen *);

struct weston_surface *
ivi_layout_surface_get_weston_surface(struct ivi_layout_surface *ivisurf);

int32_t
ivi_layout_layer_set_transition(struct ivi_layout_layer *ivilayer,
				enum ivi_layout_transition_type type,
				uint32_t duration);

int32_t
ivi_layout_layer_set_fade_info(struct ivi_layout_layer* layer,
			       uint32_t is_fade_in,
			       double start_alpha, double end_alpha);

int32_t
ivi_layout_surface_set_transition(struct ivi_layout_surface *ivisurf,
				  enum ivi_layout_transition_type type,
				  uint32_t duration);

void
ivi_layout_transition_layer_render_order(struct ivi_layout_layer* layer,
					 struct ivi_layout_surface** new_order,
					 uint32_t surface_num,
					 uint32_t duration);

void
ivi_layout_transition_move_layer_cancel(struct ivi_layout_layer* layer);

/**
 * \brief Commit all changes and execute all enqueued commands since
 * last commit.
 *
 * \return IVI_SUCCEEDED if the method call was successful
 * \return IVI_FAILED if the method call was failed
 */
int32_t
ivi_layout_commit_changes(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _IVI_LAYOUT_EXPORT_H_ */

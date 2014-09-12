// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DRAW_PROPERTIES_H_
#define CC_LAYERS_DRAW_PROPERTIES_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace cc {

// Container for properties that layers need to compute before they can be
// drawn.
template <typename LayerType>
struct CC_EXPORT DrawProperties {
  DrawProperties()
      : opacity(0.f),
        opacity_is_animating(false),
        screen_space_opacity_is_animating(false),
        target_space_transform_is_animating(false),
        screen_space_transform_is_animating(false),
        can_use_lcd_text(false),
        is_clipped(false),
        render_target(NULL),
        contents_scale_x(1.f),
        contents_scale_y(1.f),
        num_unclipped_descendants(0),
        layer_or_descendant_has_copy_request(false),
        layer_or_descendant_has_input_handler(false),
        index_of_first_descendants_addition(0),
        num_descendants_added(0),
        index_of_first_render_surface_layer_list_addition(0),
        num_render_surfaces_added(0),
        last_drawn_render_surface_layer_list_id(0),
        ideal_contents_scale(0.f),
        maximum_animation_contents_scale(0.f),
        page_scale_factor(0.f),
        device_scale_factor(0.f),
        children_need_sorting(false),
        sequence_number(0),
        sort_weight(0),
        sort_weight_sequence_number(0),
        depth(0) {}

  // Transforms objects from content space to target surface space, where
  // this layer would be drawn.
  gfx::Transform target_space_transform;

  // Transforms objects from content space to screen space (viewport space).
  gfx::Transform screen_space_transform;

  // DrawProperties::opacity may be different than LayerType::opacity,
  // particularly in the case when a RenderSurface re-parents the layer's
  // opacity, or when opacity is compounded by the hierarchy.
  float opacity;

  // xxx_is_animating flags are used to indicate whether the DrawProperties
  // are actually meaningful on the main thread. When the properties are
  // animating, the main thread may not have the same values that are used
  // to draw.
  bool opacity_is_animating;
  bool screen_space_opacity_is_animating;
  bool target_space_transform_is_animating;
  bool screen_space_transform_is_animating;

  // True if the layer can use LCD text.
  bool can_use_lcd_text;

  // True if the layer needs to be clipped by clip_rect.
  bool is_clipped;

  // The layer whose coordinate space this layer draws into. This can be
  // either the same layer (draw_properties_.render_target == this) or an
  // ancestor of this layer.
  LayerType* render_target;

  // The surface that this layer and its subtree would contribute to.
  scoped_ptr<typename LayerType::RenderSurfaceType> render_surface;

  // This rect is in the layer's content space.
  gfx::Rect visible_content_rect;

  // In target surface space, the rect that encloses the clipped, drawable
  // content of the layer.
  gfx::Rect drawable_content_rect;

  // In target surface space, the original rect that clipped this layer. This
  // value is used to avoid unnecessarily changing GL scissor state.
  gfx::Rect clip_rect;

  // The scale used to move between layer space and content space, and bounds
  // of the space. One is always a function of the other, but which one
  // depends on the layer type. For picture layers, this is an ideal scale,
  // and not always the one used.
  float contents_scale_x;
  float contents_scale_y;
  gfx::Size content_bounds;

  // Number of descendants with a clip parent that is our ancestor. NB - this
  // does not include our clip children because they are clipped by us.
  int num_unclipped_descendants;

  // If true, the layer or some layer in its sub-tree has a CopyOutputRequest
  // present on it.
  bool layer_or_descendant_has_copy_request;

  // If true, the layer or one of its descendants has a wheel or touch handler.
  bool layer_or_descendant_has_input_handler;

  // If this layer is visited out of order, its contribution to the descendant
  // and render surface layer lists will be put aside in a temporary list.
  // These values will allow for an efficient reordering of these additions.
  size_t index_of_first_descendants_addition;
  size_t num_descendants_added;
  size_t index_of_first_render_surface_layer_list_addition;
  size_t num_render_surfaces_added;

  // Each time we generate a new render surface layer list, an ID is used to
  // identify it. |last_drawn_render_surface_layer_list_id| is set to the ID
  // that marked the render surface layer list generation which last updated
  // these draw properties and determined that this layer will draw itself.
  // If these draw properties are not a part of the render surface layer list,
  // or the layer doesn't contribute anything, then this ID will be either out
  // of date or 0.
  int last_drawn_render_surface_layer_list_id;

  // The scale at which content for the layer should be rastered in order to be
  // perfectly crisp.
  float ideal_contents_scale;

  // The maximum scale during the layers current animation at which content
  // should be rastered at to be crisp.
  float maximum_animation_contents_scale;

  // The page scale factor that is applied to the layer. Since some layers may
  // have page scale applied and others not, this may differ between layers.
  float page_scale_factor;

  // The device scale factor that is applied to the layer.
  float device_scale_factor;

  // This is used to detect cases when a layer needs to sort its children to
  // ensure that scroll parents are visited before scroll children.
  bool children_need_sorting;

  // This sequence number is used to determine if we've visited this layer in
  // PreCalculateMetaInformation.
  int sequence_number;

  // Determines the order the layer should be processed in
  // CalculateDrawProperties wrt to its siblings. If you have lower
  // |sort_weight| you get visited first.
  int sort_weight;

  // This sequence number is used to determine if a sort weight has already been
  // assigned for a layer. We may assign a sort weight before we visit a layer,
  // and it's important that we don't clobber it when we do eventually visit the
  // layer.
  int sort_weight_sequence_number;

  // This is the topological depth in the tree. (E.g., root has depth zero).
  // This is used to make it faster to find the lowest common ancestor of two
  // layers.
  int depth;
};

}  // namespace cc

#endif  // CC_LAYERS_DRAW_PROPERTIES_H_

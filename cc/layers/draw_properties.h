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
template <typename LayerType, typename RenderSurfaceType>
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
        num_descendants_that_draw_content(0),
        descendants_can_clip_selves(false) {}

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
  scoped_ptr<RenderSurfaceType> render_surface;

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

  // Does not include this layer itself, only its children and descendants.
  int num_descendants_that_draw_content;

  // If true, every descendant in the sub-tree can clip itself without the
  // need to use hardware sissoring or a new render target.
  bool descendants_can_clip_selves;
};

}  // namespace cc

#endif  // CC_LAYERS_DRAW_PROPERTIES_H_

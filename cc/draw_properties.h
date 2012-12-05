// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DRAW_PROPERTIES_H_
#define CC_DRAW_PROPERTIES_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace cc {

// Container for properties that layers need to compute before they can be
// drawn.
template<typename LayerType, typename RenderSurfaceType>
struct CC_EXPORT DrawProperties {
    DrawProperties()
        : opacity(0)
        , opacity_is_animating(false)
        , target_space_transform_is_animating(false)
        , screen_space_transform_is_animating(false)
        , is_clipped(false)
        , render_target(0)
    {
    }

    // Transforms objects from content space to target surface space, where
    // this layer would be drawn.
    gfx::Transform target_space_transform;

    // Transforms objects from content space to screen space (viewport space).
    gfx::Transform screen_space_transform;

    // DrawProperties::opacity may be different than LayerType::opacity,
    // particularly in the case when a renderSurface re-parents the layer's
    // opacity, or when opacity is compounded by the hierarchy.
    float opacity;

    // XXXIsAnimating flags are used to indicate whether the drawProperties
    // are actually meaningful on the main thread. When the properties are
    // animating, the main thread may not have the same values that are used
    // to draw.
    bool opacity_is_animating;
    bool target_space_transform_is_animating;
    bool screen_space_transform_is_animating;

    // True if the layer needs to be clipped by clipRect.
    bool is_clipped;

    // The layer whose coordinate space this layer draws into. This can be
    // either the same layer (m_drawProperties.render_target == this) or an
    // ancestor of this layer.
    LayerType* render_target;

    // The surface that this layer and its subtree would contribute to.
    scoped_ptr<RenderSurfaceType> render_surface;

    // This rect is in the layer's content space.
    gfx::Rect visible_content_rect;

    // In target surface space, the rect that encloses the clipped, drawable
    // content of the layer.
    gfx::Rect drawable_content_rect;

    // In target surface space, the original rect that clipped this
    // layer. This value is used to avoid unnecessarily changing GL scissor
    // state.
    gfx::Rect clip_rect;
};

}  // namespace cc

#endif  // CC_DRAW_PROPERTIES_H_

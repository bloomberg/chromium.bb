// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_surface.h"

#include "cc/layer.h"
#include "cc/math_util.h"
#include "ui/gfx/transform.h"

namespace cc {

RenderSurface::RenderSurface(Layer* owning_layer)
    : owning_layer_(owning_layer),
      draw_opacity_(1),
      draw_opacity_is_animating_(false),
      target_surface_transforms_are_animating_(false),
      screen_space_transforms_are_animating_(false),
      is_clipped_(false),
      nearest_ancestor_that_moves_pixels_(NULL) {}

RenderSurface::~RenderSurface() {}

gfx::RectF RenderSurface::DrawableContentRect() const {
  gfx::RectF drawable_content_rect =
      MathUtil::mapClippedRect(draw_transform_, content_rect_);
  if (owning_layer_->has_replica())
    drawable_content_rect.Union(
        MathUtil::mapClippedRect(replica_draw_transform_, content_rect_));
  return drawable_content_rect;
}

}  // namespace cc

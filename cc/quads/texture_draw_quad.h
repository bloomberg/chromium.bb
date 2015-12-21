// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_TEXTURE_DRAW_QUAD_H_
#define CC_QUADS_TEXTURE_DRAW_QUAD_H_

#include <stddef.h>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/quads/draw_quad.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

class CC_EXPORT TextureDrawQuad : public DrawQuad {
 public:
  TextureDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              unsigned resource_id,
              bool premultiplied_alpha,
              const gfx::PointF& uv_top_left,
              const gfx::PointF& uv_bottom_right,
              SkColor background_color,
              const float vertex_opacity[4],
              bool y_flipped,
              bool nearest_neighbor);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              unsigned resource_id,
              gfx::Size resource_size_in_pixels,
              bool premultiplied_alpha,
              const gfx::PointF& uv_top_left,
              const gfx::PointF& uv_bottom_right,
              SkColor background_color,
              const float vertex_opacity[4],
              bool y_flipped,
              bool nearest_neighbor);

  bool premultiplied_alpha;
  gfx::PointF uv_top_left;
  gfx::PointF uv_bottom_right;
  SkColor background_color;
  float vertex_opacity[4];
  bool y_flipped;
  bool nearest_neighbor;

  struct OverlayResources {
    OverlayResources();

    gfx::Size size_in_pixels[Resources::kMaxResourceIdCount];
  };
  OverlayResources overlay_resources;

  ResourceId resource_id() const { return resources.ids[kResourceIdIndex]; }
  const gfx::Size& resource_size_in_pixels() const {
    return overlay_resources.size_in_pixels[kResourceIdIndex];
  }
  void set_resource_size_in_pixels(const gfx::Size& size_in_pixels) {
    overlay_resources.size_in_pixels[kResourceIdIndex] = size_in_pixels;
  }

  static const TextureDrawQuad* MaterialCast(const DrawQuad*);

 private:
  static const size_t kResourceIdIndex = 0;

  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace cc

#endif  // CC_QUADS_TEXTURE_DRAW_QUAD_H_

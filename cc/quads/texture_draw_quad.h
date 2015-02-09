// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_TEXTURE_DRAW_QUAD_H_
#define CC_QUADS_TEXTURE_DRAW_QUAD_H_

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
              bool flipped,
              bool nearest_neighbor);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& opaque_rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              unsigned resource_id,
              bool premultiplied_alpha,
              const gfx::PointF& uv_top_left,
              const gfx::PointF& uv_bottom_right,
              SkColor background_color,
              const float vertex_opacity[4],
              bool flipped,
              bool nearest_neighbor);

  unsigned resource_id;
  bool premultiplied_alpha;
  gfx::PointF uv_top_left;
  gfx::PointF uv_bottom_right;
  SkColor background_color;
  float vertex_opacity[4];
  bool flipped;
  bool nearest_neighbor;

  void IterateResources(const ResourceIteratorCallback& callback) override;

  static const TextureDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace cc

#endif  // CC_QUADS_TEXTURE_DRAW_QUAD_H_

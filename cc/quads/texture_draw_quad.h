// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_TEXTURE_DRAW_QUAD_H_
#define CC_QUADS_TEXTURE_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/quads/draw_quad.h"
#include "ui/gfx/rect_f.h"

namespace cc {

class CC_EXPORT TextureDrawQuad : public DrawQuad {
 public:
  static scoped_ptr<TextureDrawQuad> Create();

  void SetNew(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              unsigned resource_id,
              bool premultiplied_alpha,
              gfx::PointF uv_top_left,
              gfx::PointF uv_bottom_right,
              const float vertex_opacity[4],
              bool flipped);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              unsigned resource_id,
              bool premultiplied_alpha,
              gfx::PointF uv_top_left,
              gfx::PointF uv_bottom_right,
              const float vertex_opacity[4],
              bool flipped);

  unsigned resource_id;
  bool premultiplied_alpha;
  gfx::PointF uv_top_left;
  gfx::PointF uv_bottom_right;
  float vertex_opacity[4];
  bool flipped;

  virtual void IterateResources(const ResourceIteratorCallback& callback)
      OVERRIDE;

  static const TextureDrawQuad* MaterialCast(const DrawQuad*);

  bool PerformClipping();

 private:
  TextureDrawQuad();
};

}  // namespace cc

#endif  // CC_QUADS_TEXTURE_DRAW_QUAD_H_

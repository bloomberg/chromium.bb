// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_DRAW_QUAD_H_
#define CC_TEXTURE_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
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
              const gfx::RectF& uv_rect,
              const float vertex_opacity[4],
              bool flipped);

  void SetAll(const SharedQuadState* shared_quad_state,
              gfx::Rect rect,
              gfx::Rect opaque_rect,
              gfx::Rect visible_rect,
              bool needs_blending,
              unsigned resource_id,
              bool premultiplied_alpha,
              const gfx::RectF& uv_rect,
              const float vertex_opacity[4],
              bool flipped);

  unsigned resource_id;
  bool premultiplied_alpha;
  gfx::RectF uv_rect;
  float vertex_opacity[4];
  bool flipped;

  virtual void AppendResources(ResourceProvider::ResourceIdArray* resources)
      OVERRIDE;

  static const TextureDrawQuad* MaterialCast(const DrawQuad*);

  bool PerformClipping();
 private:
  TextureDrawQuad();
};

}

#endif  // CC_TEXTURE_DRAW_QUAD_H_

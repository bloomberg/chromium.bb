// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_CONTENT_DRAW_QUAD_BASE_H_
#define CC_QUADS_CONTENT_DRAW_QUAD_BASE_H_

#include <memory>

#include "cc/cc_export.h"
#include "components/viz/common/quads/draw_quad.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}

namespace cc {

class CC_EXPORT ContentDrawQuadBase : public viz::DrawQuad {
 public:
  void SetNew(const viz::SharedQuadState* shared_quad_state,
              viz::DrawQuad::Material material,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool swizzle_contents,
              bool nearest_neighbor);

  void SetAll(const viz::SharedQuadState* shared_quad_state,
              viz::DrawQuad::Material material,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::RectF& tex_coord_rect,
              const gfx::Size& texture_size,
              bool swizzle_contents,
              bool nearest_neighbor);

  gfx::RectF tex_coord_rect;
  gfx::Size texture_size;
  bool swizzle_contents;
  bool nearest_neighbor;

 protected:
  ContentDrawQuadBase();
  ~ContentDrawQuadBase() override;
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace cc

#endif  // CC_QUADS_CONTENT_DRAW_QUAD_BASE_H_

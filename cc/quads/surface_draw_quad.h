// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_SURFACE_DRAW_QUAD_H_
#define CC_QUADS_SURFACE_DRAW_QUAD_H_

#include <memory>

#include "cc/cc_export.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace cc {

enum class SurfaceDrawQuadType { PRIMARY, FALLBACK, LAST = FALLBACK };

class CC_EXPORT SurfaceDrawQuad : public viz::DrawQuad {
 public:
  SurfaceDrawQuad();

  void SetNew(const viz::SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              const viz::SurfaceId& surface_id,
              SurfaceDrawQuadType surface_draw_quad_type,
              SurfaceDrawQuad* fallback_quad);

  void SetAll(const viz::SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const viz::SurfaceId& surface_id,
              SurfaceDrawQuadType surface_draw_quad_type,
              SurfaceDrawQuad* fallback_quad);

  viz::SurfaceId surface_id;
  SurfaceDrawQuadType surface_draw_quad_type;
  const SurfaceDrawQuad* fallback_quad = nullptr;

  static const SurfaceDrawQuad* MaterialCast(const viz::DrawQuad* quad);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace cc

#endif  // CC_QUADS_SURFACE_DRAW_QUAD_H_

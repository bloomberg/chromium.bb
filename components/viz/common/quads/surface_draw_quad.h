// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SURFACE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SURFACE_DRAW_QUAD_H_

#include <memory>

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

enum class SurfaceDrawQuadType { PRIMARY, FALLBACK, LAST = FALLBACK };

class VIZ_COMMON_EXPORT SurfaceDrawQuad : public DrawQuad {
 public:
  SurfaceDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              const SurfaceId& surface_id,
              SurfaceDrawQuadType surface_draw_quad_type,
              SurfaceDrawQuad* fallback_quad);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const SurfaceId& surface_id,
              SurfaceDrawQuadType surface_draw_quad_type,
              SurfaceDrawQuad* fallback_quad);

  SurfaceId surface_id;
  SurfaceDrawQuadType surface_draw_quad_type;
  const SurfaceDrawQuad* fallback_quad = nullptr;

  static const SurfaceDrawQuad* MaterialCast(const DrawQuad* quad);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SURFACE_DRAW_QUAD_H_

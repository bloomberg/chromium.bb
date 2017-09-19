// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/surface_draw_quad.h"

#include "base/logging.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"

namespace viz {

SurfaceDrawQuad::SurfaceDrawQuad() = default;

void SurfaceDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             const SurfaceId& surface_id,
                             SurfaceDrawQuadType surface_draw_quad_type,
                             SkColor default_background_color,
                             SurfaceDrawQuad* fallback_quad) {
  bool needs_blending = true;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SURFACE_CONTENT, rect,
                   visible_rect, needs_blending);
  this->surface_id = surface_id;
  this->surface_draw_quad_type = surface_draw_quad_type;
  this->default_background_color = default_background_color;
  this->fallback_quad = fallback_quad;
}

void SurfaceDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const SurfaceId& surface_id,
                             SurfaceDrawQuadType surface_draw_quad_type,
                             SkColor default_background_color,
                             SurfaceDrawQuad* fallback_quad) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SURFACE_CONTENT, rect,
                   visible_rect, needs_blending);
  this->surface_id = surface_id;
  this->surface_draw_quad_type = surface_draw_quad_type;
  this->default_background_color = default_background_color;
  this->fallback_quad = fallback_quad;
}

const SurfaceDrawQuad* SurfaceDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::SURFACE_CONTENT);
  return static_cast<const SurfaceDrawQuad*>(quad);
}

void SurfaceDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  value->SetString("surface_id", surface_id.ToString());
}

}  // namespace viz

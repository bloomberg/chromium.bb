// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/surface_draw_quad.h"

#include "base/logging.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"

namespace cc {

SurfaceDrawQuad::SurfaceDrawQuad() {
}

void SurfaceDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             const viz::SurfaceId& surface_id,
                             SurfaceDrawQuadType surface_draw_quad_type,
                             SurfaceDrawQuad* fallback_quad) {
  gfx::Rect opaque_rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SURFACE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->surface_id = surface_id;
  this->surface_draw_quad_type = surface_draw_quad_type;
  this->fallback_quad = fallback_quad;
}

void SurfaceDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& opaque_rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const viz::SurfaceId& surface_id,
                             SurfaceDrawQuadType surface_draw_quad_type,
                             SurfaceDrawQuad* fallback_quad) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SURFACE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->surface_id = surface_id;
  this->surface_draw_quad_type = surface_draw_quad_type;
  this->fallback_quad = fallback_quad;
}

const SurfaceDrawQuad* SurfaceDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::SURFACE_CONTENT);
  return static_cast<const SurfaceDrawQuad*>(quad);
}

void SurfaceDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  value->SetString("surface_id", surface_id.ToString());
}


}  // namespace cc

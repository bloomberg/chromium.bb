// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/solid_color_draw_quad.h"

#include "base/logging.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"

namespace cc {

SolidColorDrawQuad::SolidColorDrawQuad()
    : color(0), force_anti_aliasing_off(false) {}

void SolidColorDrawQuad::SetNew(const viz::SharedQuadState* shared_quad_state,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                SkColor color,
                                bool force_anti_aliasing_off) {
  bool needs_blending = SkColorGetA(color) != 255;
  viz::DrawQuad::SetAll(shared_quad_state, viz::DrawQuad::SOLID_COLOR, rect,
                        visible_rect, needs_blending);
  this->color = color;
  this->force_anti_aliasing_off = force_anti_aliasing_off;
}

void SolidColorDrawQuad::SetAll(const viz::SharedQuadState* shared_quad_state,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                bool needs_blending,
                                SkColor color,
                                bool force_anti_aliasing_off) {
  viz::DrawQuad::SetAll(shared_quad_state, viz::DrawQuad::SOLID_COLOR, rect,
                        visible_rect, needs_blending);
  this->color = color;
  this->force_anti_aliasing_off = force_anti_aliasing_off;
}

const SolidColorDrawQuad* SolidColorDrawQuad::MaterialCast(
    const viz::DrawQuad* quad) {
  DCHECK(quad->material == viz::DrawQuad::SOLID_COLOR);
  return static_cast<const SolidColorDrawQuad*>(quad);
}

void SolidColorDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetInteger("color", color);
  value->SetBoolean("force_anti_aliasing_off", force_anti_aliasing_off);
}

}  // namespace cc

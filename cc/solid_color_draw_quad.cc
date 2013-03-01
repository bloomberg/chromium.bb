// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/solid_color_draw_quad.h"

#include "base/logging.h"

namespace cc {

SolidColorDrawQuad::SolidColorDrawQuad() : color(0) {}

scoped_ptr<SolidColorDrawQuad> SolidColorDrawQuad::Create() {
  return make_scoped_ptr(new SolidColorDrawQuad);
}

void SolidColorDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                gfx::Rect rect,
                                SkColor color) {
  gfx::Rect opaque_rect = SkColorGetA(color) == 255 ? rect : gfx::Rect();
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SOLID_COLOR, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
}

void SolidColorDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                gfx::Rect rect,
                                gfx::Rect opaque_rect,
                                gfx::Rect visible_rect,
                                bool needs_blending,
                                SkColor color) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SOLID_COLOR, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
}

void SolidColorDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {}

const SolidColorDrawQuad* SolidColorDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::SOLID_COLOR);
  return static_cast<const SolidColorDrawQuad*>(quad);
}

}  // namespacec cc

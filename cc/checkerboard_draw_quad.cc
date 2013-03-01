// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/checkerboard_draw_quad.h"

#include "base/logging.h"

namespace cc {

CheckerboardDrawQuad::CheckerboardDrawQuad() : color(0) {}

scoped_ptr<CheckerboardDrawQuad> CheckerboardDrawQuad::Create() {
  return make_scoped_ptr(new CheckerboardDrawQuad);
}

void CheckerboardDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                  gfx::Rect rect,
                                  SkColor color) {
  gfx::Rect opaque_rect = SkColorGetA(color) == 255 ? rect : gfx::Rect();
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::CHECKERBOARD, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
}

void CheckerboardDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                  gfx::Rect rect,
                                  gfx::Rect opaque_rect,
                                  gfx::Rect visible_rect,
                                  bool needs_blending,
                                  SkColor color) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::CHECKERBOARD, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
}

void CheckerboardDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {}

const CheckerboardDrawQuad* CheckerboardDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::CHECKERBOARD);
  return static_cast<const CheckerboardDrawQuad*>(quad);
}

}  // namespace cc

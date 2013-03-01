// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug_border_draw_quad.h"

#include "base/logging.h"

namespace cc {

DebugBorderDrawQuad::DebugBorderDrawQuad()
    : color(0),
      width(0) {
}

scoped_ptr<DebugBorderDrawQuad> DebugBorderDrawQuad::Create() {
  return make_scoped_ptr(new DebugBorderDrawQuad);
}

void DebugBorderDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                 gfx::Rect rect,
                                 SkColor color,
                                 int width) {
  gfx::Rect opaque_rect;
  gfx::Rect visible_rect = rect;
  bool needs_blending = SkColorGetA(color) < 255;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::DEBUG_BORDER, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
  this->width = width;
}

void DebugBorderDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                 gfx::Rect rect,
                                 gfx::Rect opaque_rect,
                                 gfx::Rect visible_rect,
                                 bool needs_blending,
                                 SkColor color,
                                 int width) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::DEBUG_BORDER, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->color = color;
  this->width = width;
}

void DebugBorderDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {}

const DebugBorderDrawQuad* DebugBorderDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::DEBUG_BORDER);
  return static_cast<const DebugBorderDrawQuad*>(quad);
}

}  // namespace cc

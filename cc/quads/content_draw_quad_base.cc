// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/content_draw_quad_base.h"

#include "base/logging.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"

namespace cc {

ContentDrawQuadBase::ContentDrawQuadBase()
    : swizzle_contents(false) {
}

ContentDrawQuadBase::~ContentDrawQuadBase() {
}

void ContentDrawQuadBase::SetNew(const viz::SharedQuadState* shared_quad_state,
                                 viz::DrawQuad::Material material,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 const gfx::RectF& tex_coord_rect,
                                 const gfx::Size& texture_size,
                                 bool swizzle_contents,
                                 bool nearest_neighbor) {
  viz::DrawQuad::SetAll(shared_quad_state, material, rect, visible_rect,
                        needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
  this->nearest_neighbor = nearest_neighbor;
}

void ContentDrawQuadBase::SetAll(const viz::SharedQuadState* shared_quad_state,
                                 viz::DrawQuad::Material material,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 const gfx::RectF& tex_coord_rect,
                                 const gfx::Size& texture_size,
                                 bool swizzle_contents,
                                 bool nearest_neighbor) {
  viz::DrawQuad::SetAll(shared_quad_state, material, rect, visible_rect,
                        needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
  this->nearest_neighbor = nearest_neighbor;
}

void ContentDrawQuadBase::ExtendValue(
    base::trace_event::TracedValue* value) const {
  MathUtil::AddToTracedValue("tex_coord_rect", tex_coord_rect, value);
  MathUtil::AddToTracedValue("texture_size", texture_size, value);

  value->SetBoolean("swizzle_contents", swizzle_contents);
  value->SetBoolean("nearest_neighbor", nearest_neighbor);
}

}  // namespace cc

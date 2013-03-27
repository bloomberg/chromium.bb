// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/content_draw_quad_base.h"

#include "base/logging.h"

namespace cc {

ContentDrawQuadBase::ContentDrawQuadBase()
    : swizzle_contents(false) {
}

ContentDrawQuadBase::~ContentDrawQuadBase() {
}

void ContentDrawQuadBase::SetNew(const SharedQuadState* shared_quad_state,
                                 DrawQuad::Material material,
                                 gfx::Rect rect,
                                 gfx::Rect opaque_rect,
                                 const gfx::RectF& tex_coord_rect,
                                 gfx::Size texture_size,
                                 bool swizzle_contents) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, material, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
}

void ContentDrawQuadBase::SetAll(const SharedQuadState* shared_quad_state,
                                 DrawQuad::Material material,
                                 gfx::Rect rect,
                                 gfx::Rect opaque_rect,
                                 gfx::Rect visible_rect,
                                 bool needs_blending,
                                 const gfx::RectF& tex_coord_rect,
                                 gfx::Size texture_size,
                                 bool swizzle_contents) {
  DrawQuad::SetAll(shared_quad_state, material, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
}

}  // namespace cc

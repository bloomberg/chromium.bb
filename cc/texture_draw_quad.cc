// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_draw_quad.h"

#include "base/logging.h"

namespace cc {

TextureDrawQuad::TextureDrawQuad()
    : resource_id(0),
      premultiplied_alpha(false),
      flipped(false) {
}

scoped_ptr<TextureDrawQuad> TextureDrawQuad::Create() {
  return make_scoped_ptr(new TextureDrawQuad);
}

void TextureDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             gfx::Rect rect,
                             gfx::Rect opaque_rect,
                             unsigned resource_id,
                             bool premultiplied_alpha,
                             const gfx::RectF& uv_rect,
                             bool flipped) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_rect = uv_rect;
  this->flipped = flipped;
}

void TextureDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             gfx::Rect rect,
                             gfx::Rect opaque_rect,
                             gfx::Rect visible_rect,
                             bool needs_blending,
                             unsigned resource_id,
                             bool premultiplied_alpha,
                             const gfx::RectF& uv_rect,
                             bool flipped) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_rect = uv_rect;
  this->flipped = flipped;
}

const TextureDrawQuad* TextureDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::TEXTURE_CONTENT);
  return static_cast<const TextureDrawQuad*>(quad);
}

}  // namespace cc

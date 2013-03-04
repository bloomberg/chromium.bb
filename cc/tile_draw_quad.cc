// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_draw_quad.h"

#include "base/logging.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

TileDrawQuad::TileDrawQuad()
    : resource_id(0),
      swizzle_contents(false) {
}

scoped_ptr<TileDrawQuad> TileDrawQuad::Create() {
    return make_scoped_ptr(new TileDrawQuad);
}

void TileDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                          gfx::Rect rect,
                          gfx::Rect opaque_rect,
                          unsigned resource_id,
                          const gfx::RectF& tex_coord_rect,
                          gfx::Size texture_size,
                          bool swizzle_contents) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TILED_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
}

void TileDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                          gfx::Rect rect,
                          gfx::Rect opaque_rect,
                          gfx::Rect visible_rect,
                          bool needs_blending,
                          unsigned resource_id,
                          const gfx::RectF& tex_coord_rect,
                          gfx::Size texture_size,
                          bool swizzle_contents) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TILED_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
}

void TileDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  resource_id = callback.Run(resource_id);
}

const TileDrawQuad* TileDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::TILED_CONTENT);
  return static_cast<const TileDrawQuad*>(quad);
}

}  // namespace cc

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_draw_quad.h"

#include "base/logging.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

TileDrawQuad::TileDrawQuad()
    : resource_id(0),
      swizzle_contents(false),
      left_edge_aa(false),
      top_edge_aa(false),
      right_edge_aa(false),
      bottom_edge_aa(false) {
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
                          bool swizzle_contents,
                          bool left_edge_aa,
                          bool top_edge_aa,
                          bool right_edge_aa,
                          bool bottom_edge_aa) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TILED_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
  this->left_edge_aa = left_edge_aa;
  this->top_edge_aa = top_edge_aa;
  this->right_edge_aa = right_edge_aa;
  this->bottom_edge_aa = bottom_edge_aa;

  // Override needs_blending after initializing the quad.
  this->needs_blending = IsAntialiased();
}

void TileDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                          gfx::Rect rect,
                          gfx::Rect opaque_rect,
                          gfx::Rect visible_rect,
                          bool needs_blending,
                          unsigned resource_id,
                          const gfx::RectF& tex_coord_rect,
                          gfx::Size texture_size,
                          bool swizzle_contents,
                          bool left_edge_aa,
                          bool top_edge_aa,
                          bool right_edge_aa,
                          bool bottom_edge_aa) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TILED_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->tex_coord_rect = tex_coord_rect;
  this->texture_size = texture_size;
  this->swizzle_contents = swizzle_contents;
  this->left_edge_aa = left_edge_aa;
  this->top_edge_aa = top_edge_aa;
  this->right_edge_aa = right_edge_aa;
  this->bottom_edge_aa = bottom_edge_aa;
}

void TileDrawQuad::AppendResources(
    ResourceProvider::ResourceIdArray* resources) {
  resources->push_back(resource_id);
}

const TileDrawQuad* TileDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::TILED_CONTENT);
  return static_cast<const TileDrawQuad*>(quad);
}

}  // namespace cc

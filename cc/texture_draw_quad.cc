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
                             gfx::Rect rect, gfx::Rect opaque_rect,
                             unsigned resource_id, bool premultiplied_alpha,
                             const gfx::RectF& uv_rect,
                             const float vertex_opacity[4], bool flipped) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = vertex_opacity[0] != 1.0f || vertex_opacity[1] != 1.0f
      || vertex_opacity[2] != 1.0f || vertex_opacity[3] != 1.0f;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_rect = uv_rect;
  this->vertex_opacity[0] = vertex_opacity[0];
  this->vertex_opacity[1] = vertex_opacity[1];
  this->vertex_opacity[2] = vertex_opacity[2];
  this->vertex_opacity[3] = vertex_opacity[3];
  this->flipped = flipped;
}

void TextureDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             gfx::Rect rect, gfx::Rect opaque_rect,
                             gfx::Rect visible_rect, bool needs_blending,
                             unsigned resource_id, bool premultiplied_alpha,
                             const gfx::RectF& uv_rect,
                             const float vertex_opacity[4], bool flipped) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_rect = uv_rect;
  this->vertex_opacity[0] = vertex_opacity[0];
  this->vertex_opacity[1] = vertex_opacity[1];
  this->vertex_opacity[2] = vertex_opacity[2];
  this->vertex_opacity[3] = vertex_opacity[3];
  this->flipped = flipped;
}

void TextureDrawQuad::AppendResources(
    ResourceProvider::ResourceIdArray* resources) {
  resources->push_back(resource_id);
}

const TextureDrawQuad* TextureDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::TEXTURE_CONTENT);
  return static_cast<const TextureDrawQuad*>(quad);
}

bool TextureDrawQuad::PerformClipping() {
  // This only occurs if the rect is only scaled and translated (and thus still
  // axis aligned).
  if (!quadTransform().IsScaleOrTranslation())
    return false;

  // Grab our scale and make sure it's positive.
  float x_scale = quadTransform().matrix().getDouble(0, 0);
  float y_scale = quadTransform().matrix().getDouble(1, 1);
  if (x_scale <= 0.0f || y_scale <= 0.0f)
    return false;

  // Grab our offset.
  gfx::Vector2dF offset(quadTransform().matrix().getDouble(0, 3),
                        quadTransform().matrix().getDouble(1, 3));

  // Transform the rect by the scale and offset.
  gfx::RectF rectF = rect;
  rectF.Scale(x_scale, y_scale);
  rectF += offset;

  // Perform clipping and check to see if the result is empty.
  gfx::RectF clippedRect = IntersectRects(rectF, clipRect());
  if (clippedRect.IsEmpty()) {
    rect = gfx::Rect();
    uv_rect = gfx::RectF();
    return true;
  }

  // Create a new uv-rect by clipping the old one to the new bounds.
  uv_rect = gfx::RectF(
      uv_rect.x()
          + uv_rect.width() / rectF.width() * (clippedRect.x() - rectF.x()),
      uv_rect.y()
          + uv_rect.height() / rectF.height() * (clippedRect.y() - rectF.y()),
      uv_rect.width() / rectF.width() * clippedRect.width(),
      uv_rect.height() / rectF.height() * clippedRect.height());

  // Indexing according to the quad vertex generation:
  // 1--2
  // |  |
  // 0--3
  if (vertex_opacity[0] != vertex_opacity[1]
      || vertex_opacity[0] != vertex_opacity[2]
      || vertex_opacity[0] != vertex_opacity[3]) {
    const float x1 = (clippedRect.x() - rectF.x()) / rectF.width();
    const float y1 = (clippedRect.y() - rectF.y()) / rectF.height();
    const float x3 = (clippedRect.right() - rectF.x()) / rectF.width();
    const float y3 = (clippedRect.bottom() - rectF.y()) / rectF.height();
    const float x1y1 = x1 * vertex_opacity[2] + (1.0f - x1) * vertex_opacity[1];
    const float x1y3 = x1 * vertex_opacity[3] + (1.0f - x1) * vertex_opacity[0];
    const float x3y1 = x3 * vertex_opacity[2] + (1.0f - x3) * vertex_opacity[1];
    const float x3y3 = x3 * vertex_opacity[3] + (1.0f - x3) * vertex_opacity[0];
    vertex_opacity[0] = y3 * x1y3 + (1.0f - y3) * x1y1;
    vertex_opacity[1] = y1 * x1y3 + (1.0f - y1) * x1y1;
    vertex_opacity[2] = y1 * x3y3 + (1.0f - y1) * x3y1;
    vertex_opacity[3] = y3 * x3y3 + (1.0f - y3) * x3y1;
  }

  // Move the clipped rectangle back into its space.
  clippedRect -= offset;
  clippedRect.Scale(1.0f / x_scale, 1.0f / y_scale);
  rect = gfx::Rect(static_cast<int>(clippedRect.x() + 0.5f),
                   static_cast<int>(clippedRect.y() + 0.5f),
                   static_cast<int>(clippedRect.width() + 0.5f),
                   static_cast<int>(clippedRect.height() + 0.5f));
  return true;
}

}  // namespace cc

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_draw_quad.h"

#include "base/logging.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

TextureDrawQuad::TextureDrawQuad()
    : resource_id(0),
      premultiplied_alpha(false),
      flipped(false) {
  this->vertex_opacity[0] = 0.f;
  this->vertex_opacity[1] = 0.f;
  this->vertex_opacity[2] = 0.f;
  this->vertex_opacity[3] = 0.f;
}

scoped_ptr<TextureDrawQuad> TextureDrawQuad::Create() {
  return make_scoped_ptr(new TextureDrawQuad);
}

void TextureDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             gfx::Rect rect, gfx::Rect opaque_rect,
                             unsigned resource_id, bool premultiplied_alpha,
                             gfx::PointF uv_top_left,
                             gfx::PointF uv_bottom_right,
                             const float vertex_opacity[4], bool flipped) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = vertex_opacity[0] != 1.0f || vertex_opacity[1] != 1.0f
      || vertex_opacity[2] != 1.0f || vertex_opacity[3] != 1.0f;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_top_left = uv_top_left;
  this->uv_bottom_right = uv_bottom_right;
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
                             gfx::PointF uv_top_left,
                             gfx::PointF uv_bottom_right,
                             const float vertex_opacity[4], bool flipped) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::TEXTURE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->resource_id = resource_id;
  this->premultiplied_alpha = premultiplied_alpha;
  this->uv_top_left = uv_top_left;
  this->uv_bottom_right = uv_bottom_right;
  this->vertex_opacity[0] = vertex_opacity[0];
  this->vertex_opacity[1] = vertex_opacity[1];
  this->vertex_opacity[2] = vertex_opacity[2];
  this->vertex_opacity[3] = vertex_opacity[3];
  this->flipped = flipped;
}

void TextureDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  resource_id = callback.Run(resource_id);
}

const TextureDrawQuad* TextureDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::TEXTURE_CONTENT);
  return static_cast<const TextureDrawQuad*>(quad);
}

bool TextureDrawQuad::PerformClipping() {
  // This only occurs if the rect is only scaled and translated (and thus still
  // axis aligned).
  if (!quadTransform().IsPositiveScaleOrTranslation())
    return false;

  // Grab our scale and make sure it's positive.
  float x_scale = static_cast<float>(quadTransform().matrix().getDouble(0, 0));
  float y_scale = static_cast<float>(quadTransform().matrix().getDouble(1, 1));

  // Grab our offset.
  gfx::Vector2dF offset(
      static_cast<float>(quadTransform().matrix().getDouble(0, 3)),
      static_cast<float>(quadTransform().matrix().getDouble(1, 3)));

  // Transform the rect by the scale and offset.
  gfx::RectF rectF = rect;
  rectF.Scale(x_scale, y_scale);
  rectF += offset;

  // Perform clipping and check to see if the result is empty.
  gfx::RectF clippedRect = IntersectRects(rectF, clipRect());
  if (clippedRect.IsEmpty()) {
    rect = gfx::Rect();
    uv_top_left = gfx::PointF();
    uv_bottom_right = gfx::PointF();
    return true;
  }

  // Create a new uv-rect by clipping the old one to the new bounds.
  gfx::Vector2dF uv_scale(uv_bottom_right - uv_top_left);
  uv_scale.Scale(1.f / rectF.width(), 1.f / rectF.height());
  uv_bottom_right = uv_top_left +
      gfx::ScaleVector2d(
          clippedRect.bottom_right() - rectF.origin(),
          uv_scale.x(),
          uv_scale.y());
  uv_top_left = uv_top_left +
      gfx::ScaleVector2d(
          clippedRect.origin() - rectF.origin(),
          uv_scale.x(),
          uv_scale.y());

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

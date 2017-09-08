// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/picture_draw_quad.h"

#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/resources/platform_color.h"

namespace cc {

PictureDrawQuad::PictureDrawQuad() {
}

PictureDrawQuad::PictureDrawQuad(const PictureDrawQuad& other) = default;

PictureDrawQuad::~PictureDrawQuad() {
}

void PictureDrawQuad::SetNew(const viz::SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const gfx::RectF& tex_coord_rect,
                             const gfx::Size& texture_size,
                             bool nearest_neighbor,
                             viz::ResourceFormat texture_format,
                             const gfx::Rect& content_rect,
                             float contents_scale,
                             scoped_refptr<DisplayItemList> display_item_list) {
  ContentDrawQuadBase::SetNew(
      shared_quad_state, viz::DrawQuad::PICTURE_CONTENT, rect, visible_rect,
      needs_blending, tex_coord_rect, texture_size,
      !viz::PlatformColor::SameComponentOrder(texture_format),
      nearest_neighbor);
  this->content_rect = content_rect;
  this->contents_scale = contents_scale;
  this->display_item_list = display_item_list;
  this->texture_format = texture_format;
}

void PictureDrawQuad::SetAll(const viz::SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const gfx::RectF& tex_coord_rect,
                             const gfx::Size& texture_size,
                             bool nearest_neighbor,
                             viz::ResourceFormat texture_format,
                             const gfx::Rect& content_rect,
                             float contents_scale,
                             scoped_refptr<DisplayItemList> display_item_list) {
  ContentDrawQuadBase::SetAll(
      shared_quad_state, viz::DrawQuad::PICTURE_CONTENT, rect, visible_rect,
      needs_blending, tex_coord_rect, texture_size,
      !viz::PlatformColor::SameComponentOrder(texture_format),
      nearest_neighbor);
  this->content_rect = content_rect;
  this->contents_scale = contents_scale;
  this->display_item_list = display_item_list;
  this->texture_format = texture_format;
}

const PictureDrawQuad* PictureDrawQuad::MaterialCast(
    const viz::DrawQuad* quad) {
  DCHECK(quad->material == viz::DrawQuad::PICTURE_CONTENT);
  return static_cast<const PictureDrawQuad*>(quad);
}

void PictureDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  ContentDrawQuadBase::ExtendValue(value);
  MathUtil::AddToTracedValue("content_rect", content_rect, value);
  value->SetDouble("contents_scale", contents_scale);
  value->SetInteger("texture_format", texture_format);
  // TODO(piman): display_item_list?
}

}  // namespace cc

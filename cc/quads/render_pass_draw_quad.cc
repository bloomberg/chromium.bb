// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass_draw_quad.h"

#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace cc {

RenderPassDrawQuad::RenderPassDrawQuad() {
}

RenderPassDrawQuad::RenderPassDrawQuad(const RenderPassDrawQuad& other) =
    default;

RenderPassDrawQuad::~RenderPassDrawQuad() {
}

void RenderPassDrawQuad::SetNew(const viz::SharedQuadState* shared_quad_state,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                RenderPassId render_pass_id,
                                viz::ResourceId mask_resource_id,
                                const gfx::RectF& mask_uv_rect,
                                const gfx::Size& mask_texture_size,
                                const gfx::Vector2dF& filters_scale,
                                const gfx::PointF& filters_origin,
                                const gfx::RectF& tex_coord_rect) {
  DCHECK(render_pass_id);

  bool needs_blending = true;
  SetAll(shared_quad_state, rect, visible_rect, needs_blending, render_pass_id,
         mask_resource_id, mask_uv_rect, mask_texture_size, filters_scale,
         filters_origin, tex_coord_rect);
}

void RenderPassDrawQuad::SetAll(const viz::SharedQuadState* shared_quad_state,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                bool needs_blending,
                                RenderPassId render_pass_id,
                                viz::ResourceId mask_resource_id,
                                const gfx::RectF& mask_uv_rect,
                                const gfx::Size& mask_texture_size,
                                const gfx::Vector2dF& filters_scale,
                                const gfx::PointF& filters_origin,
                                const gfx::RectF& tex_coord_rect) {
  DCHECK(render_pass_id);

  viz::DrawQuad::SetAll(shared_quad_state, viz::DrawQuad::RENDER_PASS, rect,
                        visible_rect, needs_blending);
  this->render_pass_id = render_pass_id;
  resources.ids[kMaskResourceIdIndex] = mask_resource_id;
  resources.count = mask_resource_id ? 1 : 0;
  this->mask_uv_rect = mask_uv_rect;
  this->mask_texture_size = mask_texture_size;
  this->filters_scale = filters_scale;
  this->filters_origin = filters_origin;
  this->tex_coord_rect = tex_coord_rect;
}

const RenderPassDrawQuad* RenderPassDrawQuad::MaterialCast(
    const viz::DrawQuad* quad) {
  DCHECK_EQ(quad->material, viz::DrawQuad::RENDER_PASS);
  return static_cast<const RenderPassDrawQuad*>(quad);
}

void RenderPassDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  viz::TracedValue::SetIDRef(reinterpret_cast<void*>(render_pass_id), value,
                             "render_pass_id");
  value->SetInteger("mask_resource_id", resources.ids[kMaskResourceIdIndex]);
  MathUtil::AddToTracedValue("mask_texture_size", mask_texture_size, value);
  MathUtil::AddToTracedValue("mask_uv_rect", mask_uv_rect, value);
  MathUtil::AddToTracedValue("tex_coord_rect", tex_coord_rect, value);
}

}  // namespace cc

// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass_draw_quad.h"

#include "base/debug/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace cc {

RenderPassDrawQuad::RenderPassDrawQuad()
    : render_pass_id(RenderPass::Id(-1, -1)),
      is_replica(false),
      mask_resource_id(static_cast<ResourceProvider::ResourceId>(-1)) {
}

RenderPassDrawQuad::~RenderPassDrawQuad() {
}

void RenderPassDrawQuad::SetNew(
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    RenderPass::Id render_pass_id,
    bool is_replica,
    ResourceProvider::ResourceId mask_resource_id,
    const gfx::Rect& contents_changed_since_last_frame,
    const gfx::RectF& mask_uv_rect,
    const FilterOperations& filters,
    const FilterOperations& background_filters) {
  DCHECK_GT(render_pass_id.layer_id, 0);
  DCHECK_GE(render_pass_id.index, 0);

  gfx::Rect opaque_rect;
  bool needs_blending = false;
  SetAll(shared_quad_state, rect, opaque_rect, visible_rect, needs_blending,
         render_pass_id, is_replica, mask_resource_id,
         contents_changed_since_last_frame, mask_uv_rect, filters,
         background_filters);
}

void RenderPassDrawQuad::SetAll(
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    const gfx::Rect& opaque_rect,
    const gfx::Rect& visible_rect,
    bool needs_blending,
    RenderPass::Id render_pass_id,
    bool is_replica,
    ResourceProvider::ResourceId mask_resource_id,
    const gfx::Rect& contents_changed_since_last_frame,
    const gfx::RectF& mask_uv_rect,
    const FilterOperations& filters,
    const FilterOperations& background_filters) {
  DCHECK_GT(render_pass_id.layer_id, 0);
  DCHECK_GE(render_pass_id.index, 0);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::RENDER_PASS, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->render_pass_id = render_pass_id;
  this->is_replica = is_replica;
  this->mask_resource_id = mask_resource_id;
  this->contents_changed_since_last_frame = contents_changed_since_last_frame;
  this->mask_uv_rect = mask_uv_rect;
  this->filters = filters;
  this->background_filters = background_filters;
}

void RenderPassDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  if (mask_resource_id)
    mask_resource_id = callback.Run(mask_resource_id);
}

const RenderPassDrawQuad* RenderPassDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::RENDER_PASS);
  return static_cast<const RenderPassDrawQuad*>(quad);
}

void RenderPassDrawQuad::ExtendValue(base::debug::TracedValue* value) const {
  TracedValue::SetIDRef(render_pass_id.AsTracingId(), value, "render_pass_id");
  value->SetBoolean("is_replica", is_replica);
  value->SetInteger("mask_resource_id", mask_resource_id);
  value->BeginArray("contents_changed_since_last_frame");
  MathUtil::AddToTracedValue(contents_changed_since_last_frame, value);
  value->EndArray();

  value->BeginArray("mask_uv_rect");
  MathUtil::AddToTracedValue(mask_uv_rect, value);
  value->EndArray();
  value->BeginDictionary("filters");
  filters.AsValueInto(value);
  value->EndDictionary();

  value->BeginDictionary("background_filters");
  background_filters.AsValueInto(value);
  value->EndDictionary();
}

}  // namespace cc

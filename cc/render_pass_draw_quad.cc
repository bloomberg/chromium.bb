// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_pass_draw_quad.h"

namespace cc {

RenderPassDrawQuad::RenderPassDrawQuad()
    : render_pass_id(RenderPass::Id(-1, -1)),
      is_replica(false),
      mask_resource_id(-1) {
}

RenderPassDrawQuad::~RenderPassDrawQuad() {
}

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::Create() {
    return make_scoped_ptr(new RenderPassDrawQuad);
}

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::Copy(
    const SharedQuadState* copied_shared_quad_state,
    RenderPass::Id copied_render_pass_id) const {
  scoped_ptr<RenderPassDrawQuad> copy_quad(
      new RenderPassDrawQuad(*MaterialCast(this)));
  copy_quad->shared_quad_state = copied_shared_quad_state;
  copy_quad->render_pass_id = copied_render_pass_id;
  return copy_quad.Pass();
}

void RenderPassDrawQuad::SetNew(
    const SharedQuadState* shared_quad_state,
    gfx::Rect rect,
    RenderPass::Id render_pass_id,
    bool is_replica,
    ResourceProvider::ResourceId mask_resource_id,
    gfx::Rect contents_changed_since_last_frame,
    gfx::RectF mask_uv_rect,
    const WebKit::WebFilterOperations& filters,
    skia::RefPtr<SkImageFilter> filter,
    const WebKit::WebFilterOperations& background_filters) {
  DCHECK(render_pass_id.layer_id > 0);
  DCHECK(render_pass_id.index >= 0);

  gfx::Rect opaque_rect;
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  SetAll(shared_quad_state, rect, opaque_rect, visible_rect, needs_blending,
         render_pass_id, is_replica, mask_resource_id,
         contents_changed_since_last_frame, mask_uv_rect, filters, filter,
         background_filters);
}

void RenderPassDrawQuad::SetAll(
    const SharedQuadState* shared_quad_state,
    gfx::Rect rect,
    gfx::Rect opaque_rect,
    gfx::Rect visible_rect,
    bool needs_blending,
    RenderPass::Id render_pass_id,
    bool is_replica,
    ResourceProvider::ResourceId mask_resource_id,
    gfx::Rect contents_changed_since_last_frame,
    gfx::RectF mask_uv_rect,
    const WebKit::WebFilterOperations& filters,
    skia::RefPtr<SkImageFilter> filter,
    const WebKit::WebFilterOperations& background_filters) {
  DCHECK(render_pass_id.layer_id > 0);
  DCHECK(render_pass_id.index >= 0);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::RENDER_PASS, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->render_pass_id = render_pass_id;
  this->is_replica = is_replica;
  this->mask_resource_id = mask_resource_id;
  this->contents_changed_since_last_frame = contents_changed_since_last_frame;
  this->mask_uv_rect = mask_uv_rect;
  this->filters = filters;
  this->filter = filter;
  this->background_filters = background_filters;
}

void RenderPassDrawQuad::AppendResources(
    ResourceProvider::ResourceIdArray* resources) {
  resources->push_back(mask_resource_id);
}

const RenderPassDrawQuad* RenderPassDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::RENDER_PASS);
  return static_cast<const RenderPassDrawQuad*>(quad);
}

}  // namespace cc

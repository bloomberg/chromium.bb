// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_pass_draw_quad.h"

namespace cc {

RenderPassDrawQuad::RenderPassDrawQuad()
    : render_pass_id(RenderPass::Id(-1, -1)),
      is_replica(false),
      mask_resource_id(-1),
      mask_tex_coord_scale_x(0),
      mask_tex_coord_scale_y(0),
      mask_tex_coord_offset_x(0),
      mask_tex_coord_offset_y(0) {
}

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::Create() {
    return make_scoped_ptr(new RenderPassDrawQuad);
}

scoped_ptr<RenderPassDrawQuad> RenderPassDrawQuad::Copy(
    const SharedQuadState* copied_shared_quad_state,
    RenderPass::Id copied_render_pass_id) const {
  scoped_ptr<RenderPassDrawQuad> copy_quad(new RenderPassDrawQuad(*MaterialCast(this)));
  copy_quad->shared_quad_state = copied_shared_quad_state;
  copy_quad->render_pass_id = copied_render_pass_id;
  return copy_quad.Pass();
}

void RenderPassDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                gfx::Rect rect,
                                RenderPass::Id render_pass_id,
                                bool is_replica,
                                ResourceProvider::ResourceId mask_resource_id,
                                gfx::Rect contents_changed_since_last_frame,
                                float mask_tex_coord_scale_x,
                                float mask_tex_coord_scale_y,
                                float mask_tex_coord_offset_x,
                                float mask_tex_coord_offset_y) {
  DCHECK(render_pass_id.layerId > 0);
  DCHECK(render_pass_id.index >= 0);

  gfx::Rect opaque_rect;
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::RENDER_PASS, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->render_pass_id = render_pass_id;
  this->is_replica = is_replica;
  this->mask_resource_id = mask_resource_id;
  this->contents_changed_since_last_frame = contents_changed_since_last_frame;
  this->mask_tex_coord_scale_x = mask_tex_coord_scale_x;
  this->mask_tex_coord_scale_y = mask_tex_coord_scale_y;
  this->mask_tex_coord_offset_x = mask_tex_coord_offset_x;
  this->mask_tex_coord_offset_y = mask_tex_coord_offset_y;
}

void RenderPassDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                gfx::Rect rect,
                                gfx::Rect opaque_rect,
                                gfx::Rect visible_rect,
                                bool needs_blending,
                                RenderPass::Id render_pass_id,
                                bool is_replica,
                                ResourceProvider::ResourceId mask_resource_id,
                                gfx::Rect contents_changed_since_last_frame,
                                float mask_tex_coord_scale_x,
                                float mask_tex_coord_scale_y,
                                float mask_tex_coord_offset_x,
                                float mask_tex_coord_offset_y) {
  DCHECK(render_pass_id.layerId > 0);
  DCHECK(render_pass_id.index >= 0);

  DrawQuad::SetAll(shared_quad_state, DrawQuad::RENDER_PASS, rect, opaque_rect,
                   visible_rect, needs_blending);
  this->render_pass_id = render_pass_id;
  this->is_replica = is_replica;
  this->mask_resource_id = mask_resource_id;
  this->contents_changed_since_last_frame = contents_changed_since_last_frame;
  this->mask_tex_coord_scale_x = mask_tex_coord_scale_x;
  this->mask_tex_coord_scale_y = mask_tex_coord_scale_y;
  this->mask_tex_coord_offset_x = mask_tex_coord_offset_x;
  this->mask_tex_coord_offset_y = mask_tex_coord_offset_y;
}

const RenderPassDrawQuad* RenderPassDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::RENDER_PASS);
  return static_cast<const RenderPassDrawQuad*>(quad);
}

}  // namespace cc

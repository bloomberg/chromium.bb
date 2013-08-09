// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/yuv_video_draw_quad.h"

#include "base/logging.h"
#include "base/values.h"
#include "cc/base/math_util.h"

namespace cc {

YUVVideoDrawQuad::YUVVideoDrawQuad()
    : y_plane_resource_id(0),
      u_plane_resource_id(0),
      v_plane_resource_id(0),
      a_plane_resource_id(0) {}
YUVVideoDrawQuad::~YUVVideoDrawQuad() {}

scoped_ptr<YUVVideoDrawQuad> YUVVideoDrawQuad::Create() {
  return make_scoped_ptr(new YUVVideoDrawQuad);
}

void YUVVideoDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                              gfx::Rect rect,
                              gfx::Rect opaque_rect,
                              gfx::SizeF tex_scale,
                              unsigned y_plane_resource_id,
                              unsigned u_plane_resource_id,
                              unsigned v_plane_resource_id,
                              unsigned a_plane_resource_id) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::YUV_VIDEO_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->tex_scale = tex_scale;
  this->y_plane_resource_id = y_plane_resource_id;
  this->u_plane_resource_id = u_plane_resource_id;
  this->v_plane_resource_id = v_plane_resource_id;
  this->a_plane_resource_id = a_plane_resource_id;
}

void YUVVideoDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                              gfx::Rect rect,
                              gfx::Rect opaque_rect,
                              gfx::Rect visible_rect,
                              bool needs_blending,
                              gfx::SizeF tex_scale,
                              unsigned y_plane_resource_id,
                              unsigned u_plane_resource_id,
                              unsigned v_plane_resource_id,
                              unsigned a_plane_resource_id) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::YUV_VIDEO_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->tex_scale = tex_scale;
  this->y_plane_resource_id = y_plane_resource_id;
  this->u_plane_resource_id = u_plane_resource_id;
  this->v_plane_resource_id = v_plane_resource_id;
  this->a_plane_resource_id = a_plane_resource_id;
}

void YUVVideoDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  y_plane_resource_id = callback.Run(y_plane_resource_id);
  u_plane_resource_id = callback.Run(u_plane_resource_id);
  v_plane_resource_id = callback.Run(v_plane_resource_id);
  if (a_plane_resource_id)
    a_plane_resource_id = callback.Run(a_plane_resource_id);
}

const YUVVideoDrawQuad* YUVVideoDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::YUV_VIDEO_CONTENT);
  return static_cast<const YUVVideoDrawQuad*>(quad);
}

void YUVVideoDrawQuad::ExtendValue(base::DictionaryValue* value) const {
  value->Set("tex_scale", MathUtil::AsValue(tex_scale).release());
  value->SetInteger("y_plane_resource_id", y_plane_resource_id);
  value->SetInteger("u_plane_resource_id", u_plane_resource_id);
  value->SetInteger("v_plane_resource_id", v_plane_resource_id);
  value->SetInteger("a_plane_resource_id", a_plane_resource_id);
}

}  // namespace cc

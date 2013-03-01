// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/yuv_video_draw_quad.h"

#include "base/logging.h"

namespace cc {

YUVVideoDrawQuad::YUVVideoDrawQuad() {}
YUVVideoDrawQuad::~YUVVideoDrawQuad() {}

scoped_ptr<YUVVideoDrawQuad> YUVVideoDrawQuad::Create() {
    return make_scoped_ptr(new YUVVideoDrawQuad);
}

void YUVVideoDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                              gfx::Rect rect,
                              gfx::Rect opaque_rect,
                              gfx::SizeF tex_scale,
                              const VideoLayerImpl::FramePlane& y_plane,
                              const VideoLayerImpl::FramePlane& u_plane,
                              const VideoLayerImpl::FramePlane& v_plane) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::YUV_VIDEO_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->tex_scale = tex_scale;
  this->y_plane = y_plane;
  this->u_plane = u_plane;
  this->v_plane = v_plane;
}

void YUVVideoDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                              gfx::Rect rect,
                              gfx::Rect opaque_rect,
                              gfx::Rect visible_rect,
                              bool needs_blending,
                              gfx::SizeF tex_scale,
                              const VideoLayerImpl::FramePlane& y_plane,
                              const VideoLayerImpl::FramePlane& u_plane,
                              const VideoLayerImpl::FramePlane& v_plane) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::YUV_VIDEO_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->tex_scale = tex_scale;
  this->y_plane = y_plane;
  this->u_plane = u_plane;
  this->v_plane = v_plane;
}

void YUVVideoDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  y_plane.resourceId = callback.Run(y_plane.resourceId);
  u_plane.resourceId = callback.Run(u_plane.resourceId);
  v_plane.resourceId = callback.Run(v_plane.resourceId);
}

const YUVVideoDrawQuad* YUVVideoDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::YUV_VIDEO_CONTENT);
  return static_cast<const YUVVideoDrawQuad*>(quad);
}

}  // namespace cc

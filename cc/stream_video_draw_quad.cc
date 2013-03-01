// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/stream_video_draw_quad.h"

#include "base/logging.h"

namespace cc {

StreamVideoDrawQuad::StreamVideoDrawQuad() : texture_id(0) {}

scoped_ptr<StreamVideoDrawQuad> StreamVideoDrawQuad::Create() {
  return make_scoped_ptr(new StreamVideoDrawQuad);
}

void StreamVideoDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                 gfx::Rect rect,
                                 gfx::Rect opaque_rect,
                                 unsigned texture_id,
                                 const gfx::Transform& matrix) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::STREAM_VIDEO_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->texture_id = texture_id;
  this->matrix = matrix;
}

void StreamVideoDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                 gfx::Rect rect,
                                 gfx::Rect opaque_rect,
                                 gfx::Rect visible_rect,
                                 bool needs_blending,
                                 unsigned texture_id,
                                 const gfx::Transform& matrix) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::STREAM_VIDEO_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->texture_id = texture_id;
  this->matrix = matrix;
}

void StreamVideoDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  // TODO(danakj): Convert to TextureDrawQuad?
  NOTIMPLEMENTED();
}

const StreamVideoDrawQuad* StreamVideoDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::STREAM_VIDEO_CONTENT);
  return static_cast<const StreamVideoDrawQuad*>(quad);
}

}  // namespace cc

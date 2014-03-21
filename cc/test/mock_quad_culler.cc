// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_quad_culler.h"

#include "cc/quads/draw_quad.h"

namespace cc {

MockQuadCuller::MockQuadCuller()
    : active_quad_list_(&quad_list_storage_),
      active_shared_quad_state_list_(&shared_quad_state_storage_) {}

MockQuadCuller::MockQuadCuller(
    QuadList* external_quad_list,
    SharedQuadStateList* external_shared_quad_state_list)
    : active_quad_list_(external_quad_list),
      active_shared_quad_state_list_(external_shared_quad_state_list) {}

MockQuadCuller::~MockQuadCuller() {}

SharedQuadState* MockQuadCuller::UseSharedQuadState(
    scoped_ptr<SharedQuadState> shared_quad_state) {
  SharedQuadState* raw_ptr = shared_quad_state.get();
  active_shared_quad_state_list_->push_back(shared_quad_state.Pass());
  return raw_ptr;
}

gfx::Rect MockQuadCuller::UnoccludedContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  DCHECK(draw_transform.IsIdentity() || occluded_content_rect_.IsEmpty());
  gfx::Rect result = content_rect;
  result.Subtract(occluded_content_rect_);
  return result;
}

gfx::Rect MockQuadCuller::UnoccludedContributingSurfaceContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) {
  DCHECK(draw_transform.IsIdentity() ||
         occluded_content_rect_for_contributing_surface_.IsEmpty());
  gfx::Rect result = content_rect;
  result.Subtract(occluded_content_rect_for_contributing_surface_);
  return result;
}

bool MockQuadCuller::MaybeAppend(scoped_ptr<DrawQuad> draw_quad) {
  if (!draw_quad->rect.IsEmpty()) {
    active_quad_list_->push_back(draw_quad.Pass());
    return true;
  }
  return false;
}

void MockQuadCuller::Append(scoped_ptr<DrawQuad> draw_quad) {
  DCHECK(!draw_quad->rect.IsEmpty());
  DCHECK(!draw_quad->visible_rect.IsEmpty());
  active_quad_list_->push_back(draw_quad.Pass());
}

}  // namespace cc

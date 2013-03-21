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

bool MockQuadCuller::Append(scoped_ptr<DrawQuad> draw_quad, AppendQuadsData*) {
  if (!draw_quad->rect.IsEmpty()) {
    active_quad_list_->push_back(draw_quad.Pass());
    return true;
  }
  return false;
}

SharedQuadState* MockQuadCuller::UseSharedQuadState(
    scoped_ptr<SharedQuadState> shared_quad_state) {
  SharedQuadState* raw_ptr = shared_quad_state.get();
  active_shared_quad_state_list_->push_back(shared_quad_state.Pass());
  return raw_ptr;
}

}  // namespace cc

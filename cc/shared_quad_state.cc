// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/shared_quad_state.h"

namespace cc {

SharedQuadState::SharedQuadState() : opacity(0) {}

scoped_ptr<SharedQuadState> SharedQuadState::Create() {
  return make_scoped_ptr(new SharedQuadState);
}

scoped_ptr<SharedQuadState> SharedQuadState::Copy() const {
  return make_scoped_ptr(new SharedQuadState(*this));
}

void SharedQuadState::SetAll(
    const WebKit::WebTransformationMatrix& content_to_target_transform,
    const gfx::Rect& visible_content_rect,
    const gfx::Rect& clipped_rect_in_target,
    float opacity) {
  this->content_to_target_transform = content_to_target_transform;
  this->visible_content_rect = visible_content_rect;
  this->clipped_rect_in_target = clipped_rect_in_target;
  this->opacity = opacity;
}

}  // namespace cc

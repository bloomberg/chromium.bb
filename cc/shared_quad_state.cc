// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/shared_quad_state.h"

namespace cc {

SharedQuadState::SharedQuadState() : is_clipped(false), opacity(0) {}

SharedQuadState::~SharedQuadState() {}

scoped_ptr<SharedQuadState> SharedQuadState::Create() {
  return make_scoped_ptr(new SharedQuadState);
}

scoped_ptr<SharedQuadState> SharedQuadState::Copy() const {
  return make_scoped_ptr(new SharedQuadState(*this));
}

void SharedQuadState::SetAll(
    const gfx::Transform& content_to_target_transform,
    const gfx::Size content_bounds,
    const gfx::Rect& visible_content_rect,
    const gfx::Rect& clip_rect,
    bool is_clipped,
    float opacity) {
  this->content_to_target_transform = content_to_target_transform;
  this->content_bounds = content_bounds;
  this->visible_content_rect = visible_content_rect;
  this->clip_rect = clip_rect;
  this->is_clipped = is_clipped;
  this->opacity = opacity;
}

}  // namespace cc

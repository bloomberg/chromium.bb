// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/shared_quad_state.h"

#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"

namespace cc {

SharedQuadState::SharedQuadState() : is_clipped(false), opacity(0.f) {}

SharedQuadState::~SharedQuadState() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      "cc::SharedQuadState", this);
}

scoped_ptr<SharedQuadState> SharedQuadState::Create() {
  return make_scoped_ptr(new SharedQuadState);
}

scoped_ptr<SharedQuadState> SharedQuadState::Copy() const {
  return make_scoped_ptr(new SharedQuadState(*this));
}

void SharedQuadState::SetAll(
    const gfx::Transform& content_to_target_transform,
    gfx::Size content_bounds,
    gfx::Rect visible_content_rect,
    gfx::Rect clip_rect,
    bool is_clipped,
    float opacity) {
  this->content_to_target_transform = content_to_target_transform;
  this->content_bounds = content_bounds;
  this->visible_content_rect = visible_content_rect;
  this->clip_rect = clip_rect;
  this->is_clipped = is_clipped;
  this->opacity = opacity;
}

scoped_ptr<base::Value> SharedQuadState::AsValue() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("transform",
             MathUtil::AsValue(content_to_target_transform).release());
  value->Set("layer_content_bounds",
             MathUtil::AsValue(content_bounds).release());
  value->Set("layer_visible_content_rect",
             MathUtil::AsValue(visible_content_rect).release());
  value->SetBoolean("is_clipped", is_clipped);
  value->Set("clip_rect", MathUtil::AsValue(clip_rect).release());
  value->SetDouble("opacity", opacity);
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      value.get(), "cc::SharedQuadState", this);
  return value.PassAs<base::Value>();
}

}  // namespace cc

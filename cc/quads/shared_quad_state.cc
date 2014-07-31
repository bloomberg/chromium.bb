// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/shared_quad_state.h"

#include "base/debug/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"

namespace cc {

SharedQuadState::SharedQuadState()
    : is_clipped(false),
      opacity(0.f),
      blend_mode(SkXfermode::kSrcOver_Mode),
      sorting_context_id(0) {
}

SharedQuadState::~SharedQuadState() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      "cc::SharedQuadState", this);
}

void SharedQuadState::CopyFrom(const SharedQuadState* other) {
  *this = *other;
}

void SharedQuadState::SetAll(const gfx::Transform& content_to_target_transform,
                             const gfx::Size& content_bounds,
                             const gfx::Rect& visible_content_rect,
                             const gfx::Rect& clip_rect,
                             bool is_clipped,
                             float opacity,
                             SkXfermode::Mode blend_mode,
                             int sorting_context_id) {
  this->content_to_target_transform = content_to_target_transform;
  this->content_bounds = content_bounds;
  this->visible_content_rect = visible_content_rect;
  this->clip_rect = clip_rect;
  this->is_clipped = is_clipped;
  this->opacity = opacity;
  this->blend_mode = blend_mode;
  this->sorting_context_id = sorting_context_id;
}

void SharedQuadState::AsValueInto(base::debug::TracedValue* value) const {
  value->BeginArray("transform");
  MathUtil::AddToTracedValue(content_to_target_transform, value);
  value->EndArray();

  value->BeginDictionary("layer_content_bounds");
  MathUtil::AddToTracedValue(content_bounds, value);
  value->EndDictionary();

  value->BeginArray("layer_visible_content_rect");
  MathUtil::AddToTracedValue(visible_content_rect, value);
  value->EndArray();

  value->SetBoolean("is_clipped", is_clipped);

  value->BeginArray("clip_rect");
  MathUtil::AddToTracedValue(clip_rect, value);
  value->EndArray();

  value->SetDouble("opacity", opacity);
  value->SetString("blend_mode", SkXfermode::ModeName(blend_mode));
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"),
      value,
      "cc::SharedQuadState",
      this);
}

}  // namespace cc

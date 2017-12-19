// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/linear_layout.h"

namespace vr {

namespace {

// NB: this function makes no attempt to account for rotated elements.
float GetExtent(const UiElement& element, bool horizontal) {
  gfx::Point3F p = horizontal ? gfx::Point3F(element.size().width(), 0, 0)
                              : gfx::Point3F(0, element.size().height(), 0);
  gfx::Point3F o;
  element.LocalTransform().TransformPoint(&p);
  element.LocalTransform().TransformPoint(&o);
  return (p - o).Length();
}

}  // namespace

LinearLayout::LinearLayout(Direction direction) : direction_(direction) {}
LinearLayout::~LinearLayout() {}

void LinearLayout::LayOutChildren() {
  bool horizontal =
      direction_ == LinearLayout::kLeft || direction_ == LinearLayout::kRight;
  float total_extent = -margin_;
  float minor_extent = 0;

  for (auto& child : children()) {
    if (child->requires_layout()) {
      total_extent += GetExtent(*child, horizontal) + margin_;
      minor_extent = std::max(minor_extent, GetExtent(*child, !horizontal));
    }
  }

  float x_factor = 0.f;
  float y_factor = 0.f;
  switch (direction_) {
    case kUp:
      y_factor = 1.f;
      break;
    case kDown:
      y_factor = -1.f;
      break;
    case kLeft:
      x_factor = -1.f;
      break;
    case kRight:
      x_factor = 1.f;
      break;
  }

  float cumulative_offset = -0.5 * total_extent;
  for (auto& child : children()) {
    if (!child->requires_layout())
      continue;
    float extent = GetExtent(*child, horizontal);
    float offset = cumulative_offset + 0.5 * extent;
    child->SetLayoutOffset(offset * x_factor, offset * y_factor);
    cumulative_offset += extent + margin_;
  }

  SetSize(horizontal ? total_extent : minor_extent,
          !horizontal ? total_extent : minor_extent);
}

}  // namespace vr

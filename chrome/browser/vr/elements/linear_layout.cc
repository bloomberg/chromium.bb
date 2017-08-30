// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/linear_layout.h"

namespace vr {

namespace {

float GetExtent(const UiElement& element, LinearLayout::Direction direction) {
  gfx::SizeF size = element.size();
  return direction == LinearLayout::kHorizontal ? size.width() : size.height();
}

}  // namespace

LinearLayout::LinearLayout(Direction direction) : direction_(direction) {}
LinearLayout::~LinearLayout() {}

void LinearLayout::LayOutChildren() {
  float total_extent = -margin_;
  for (auto& child : children()) {
    if (child->requires_layout())
      total_extent += GetExtent(*child, direction_) + margin_;
  }

  float offset = -0.5 * total_extent;
  for (auto& child : children()) {
    if (!child->requires_layout())
      continue;
    float extent = GetExtent(*child, direction_);
    if (direction_ == kHorizontal)
      child->SetLayoutOffset(offset + 0.5 * extent, 0);
    else
      child->SetLayoutOffset(0, offset + 0.5 * extent);
    offset += extent + margin_;
  }
}

}  // namespace vr

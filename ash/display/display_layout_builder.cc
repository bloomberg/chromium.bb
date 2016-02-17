// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_layout_builder.h"

namespace ash {

DisplayLayoutBuilder::DisplayLayoutBuilder(const DisplayLayout& layout)
    : layout_(layout.Copy()) {}

DisplayLayoutBuilder::DisplayLayoutBuilder(int64_t primary_id)
    : layout_(new DisplayLayout) {
  layout_->primary_id = primary_id;
}

DisplayLayoutBuilder::~DisplayLayoutBuilder() {}

DisplayLayoutBuilder& DisplayLayoutBuilder::SetDefaultUnified(
    bool default_unified) {
  layout_->default_unified = default_unified;
  return *this;
}

DisplayLayoutBuilder& DisplayLayoutBuilder::SetMirrored(bool mirrored) {
  layout_->mirrored = mirrored;
  return *this;
}

DisplayLayoutBuilder& DisplayLayoutBuilder::SetSecondaryPlacement(
    int64_t secondary_id,
    DisplayPlacement::Position position,
    int offset) {
  layout_->placement.position = position;
  layout_->placement.offset = offset;
  layout_->placement.display_id = secondary_id;
  layout_->placement.parent_display_id = layout_->primary_id;
  return *this;
}

scoped_ptr<DisplayLayout> DisplayLayoutBuilder::Build() {
  return std::move(layout_);
}

}  // namespace ash

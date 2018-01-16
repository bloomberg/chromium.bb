// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_snap_data.h"

namespace cc {

SnapContainerData::SnapContainerData() = default;

SnapContainerData::SnapContainerData(ScrollSnapType type)
    : scroll_snap_type_(type) {}

SnapContainerData::SnapContainerData(ScrollSnapType type, gfx::ScrollOffset max)
    : scroll_snap_type_(type), max_position_(max) {}

SnapContainerData::SnapContainerData(const SnapContainerData& other) = default;

SnapContainerData::SnapContainerData(SnapContainerData&& other)
    : scroll_snap_type_(other.scroll_snap_type_),
      max_position_(other.max_position_),
      snap_area_list_(std::move(other.snap_area_list_)) {}

SnapContainerData::~SnapContainerData() = default;

SnapContainerData& SnapContainerData::operator=(
    const SnapContainerData& other) = default;

SnapContainerData& SnapContainerData::operator=(SnapContainerData&& other) {
  scroll_snap_type_ = other.scroll_snap_type_;
  max_position_ = other.max_position_;
  snap_area_list_ = std::move(other.snap_area_list_);
  return *this;
}

void SnapContainerData::AddSnapAreaData(SnapAreaData snap_area_data) {
  snap_area_list_.push_back(snap_area_data);
}

gfx::ScrollOffset SnapContainerData::FindSnapPosition(
    const gfx::ScrollOffset& current_position,
    bool should_snap_on_x,
    bool should_snap_on_y) const {
  SnapAxis axis = scroll_snap_type_.axis;
  should_snap_on_x &= (axis == SnapAxis::kX || axis == SnapAxis::kBoth);
  should_snap_on_y &= (axis == SnapAxis::kY || axis == SnapAxis::kBoth);
  if (!should_snap_on_x && !should_snap_on_y)
    return current_position;

  float smallest_distance_x = std::numeric_limits<float>::max();
  float smallest_distance_y = std::numeric_limits<float>::max();

  gfx::ScrollOffset snap_position = current_position;

  for (const SnapAreaData& snap_area_data : snap_area_list_) {
    // TODO(sunyunjia): We should consider visiblity when choosing snap offset.
    if (should_snap_on_x && (snap_area_data.snap_axis == SnapAxis::kX ||
                             snap_area_data.snap_axis == SnapAxis::kBoth)) {
      float offset = snap_area_data.snap_position.x();
      if (offset == SnapAreaData::kInvalidScrollPosition)
        continue;
      float distance = std::abs(current_position.x() - offset);
      if (distance < smallest_distance_x) {
        smallest_distance_x = distance;
        snap_position.set_x(offset);
      }
    }
    if (should_snap_on_y && (snap_area_data.snap_axis == SnapAxis::kY ||
                             snap_area_data.snap_axis == SnapAxis::kBoth)) {
      float offset = snap_area_data.snap_position.y();
      if (offset == SnapAreaData::kInvalidScrollPosition)
        continue;
      float distance = std::abs(current_position.y() - offset);
      if (distance < smallest_distance_y) {
        smallest_distance_y = distance;
        snap_position.set_y(offset);
      }
    }
  }
  return snap_position;
}

std::ostream& operator<<(std::ostream& ostream, const SnapAreaData& area_data) {
  return ostream << area_data.snap_position.x() << ", "
                 << area_data.snap_position.y();
}

std::ostream& operator<<(std::ostream& ostream,
                         const SnapContainerData& container_data) {
  for (size_t i = 0; i < container_data.size(); ++i) {
    ostream << container_data.at(i) << "\n";
  }
  return ostream;
}

}  // namespace cc

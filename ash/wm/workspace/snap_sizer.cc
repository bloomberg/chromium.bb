// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/snap_sizer.h"

#include <cmath>

#include "ash/screen_ash.h"
#include "ash/wm/window_resizer.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// Sizes in pixel for the width of a given window.
const int kSizes[] = { 1280, 1024, 768, 640 };

// Windows are initially snapped to the size at index 0. The index into
// |kSizes| is changed if any of the following happen:
// . The user stops moving the mouse for |kDelayBeforeIncreaseMS| and then moves
//   the mouse again.
// . The mouse moves |kPixelsBeforeAdjust| horizontal pixels.
// . The mouse is against the edge of the screen and the mouse is moved
//   |kMovesBeforeAdjust| times.
const int kDelayBeforeIncreaseMS = 500;
const int kMovesBeforeAdjust = 50;
const int kPixelsBeforeAdjust = 100;

// When the smallest resolution does not fit on the screen, we take this
// fraction of the available space.
const int kMinimumScreenPercent = 90;

}  // namespace

SnapSizer::SnapSizer(aura::Window* window,
                     const gfx::Point& start,
                     Edge edge)
    : window_(window),
      edge_(edge),
      time_last_update_(base::TimeTicks::Now()),
      size_index_(0),
      resize_disabled_(false),
      num_moves_since_adjust_(0),
      last_adjust_x_(start.x()),
      last_update_x_(start.x()) {
  target_bounds_ = GetTargetBounds();
}

void SnapSizer::Update(const gfx::Point& location) {
  // See description above for details on this behavior.
  num_moves_since_adjust_++;
  if ((base::TimeTicks::Now() - time_last_update_).InMilliseconds() >
      kDelayBeforeIncreaseMS) {
    ChangeBounds(location.x(),
                 CalculateIncrement(location.x(), last_update_x_));
  } else {
    bool along_edge = AlongEdge(location.x());
    if (std::abs(location.x() - last_adjust_x_) >= kPixelsBeforeAdjust ||
        (along_edge && num_moves_since_adjust_ >= kMovesBeforeAdjust)) {
      ChangeBounds(location.x(),
                   CalculateIncrement(location.x(), last_adjust_x_));
    }
  }
  last_update_x_ = location.x();
  time_last_update_ = base::TimeTicks::Now();
}

gfx::Rect SnapSizer::GetSnapBounds(const gfx::Rect& bounds) {
  size_t current = 0;
  if (!resize_disabled_) {
    for (current = arraysize(kSizes) - 1; current; current--) {
      gfx::Rect target = GetTargetBoundsForSize(current);
      if (target == bounds) {
        ++current;
        break;
      }
    }
  }
  return GetTargetBoundsForSize(current % arraysize(kSizes));
}

void SnapSizer::SelectDefaultSizeAndDisableResize() {
  resize_disabled_ = true;
  size_index_ = 0;
  target_bounds_ = GetTargetBounds();
}

gfx::Rect SnapSizer::GetTargetBoundsForSize(size_t size_index) const {
  gfx::Rect work_area(ScreenAsh::GetUnmaximizedWorkAreaBoundsInParent(window_));
  int y = work_area.y();
  // We don't align to the bottom of the grid as the launcher may not
  // necessarily align to the grid (happens when auto-hidden).
  int max_y = work_area.bottom();
  int width = kSizes[size_index];
  if (resize_disabled_) {
    width = std::max(1024, work_area.width() / 2);
  } else {
    while (width >= work_area.width()) {
      if (size_index >= arraysize(kSizes))
        break;
      width = kSizes[++size_index];
    }
  }

  // Make sure that we keep the size of the window smaller then a certain
  // fraction of the screen space.
  width = std::min(width, kMinimumScreenPercent * work_area.width() / 100);

  if (edge_ == LEFT_EDGE) {
    int x = work_area.x();
    int mid_x = x + width;
    return gfx::Rect(x, y, mid_x - x, max_y - y);
  }
  int max_x = work_area.right();
  int x = max_x - width;
  return gfx::Rect(x , y, max_x - x, max_y - y);
}

int SnapSizer::CalculateIncrement(int x, int reference_x) const {
  if (AlongEdge(x))
    return 1;
  if (x == reference_x)
    return 0;
  if (edge_ == LEFT_EDGE) {
    if (x < reference_x)
      return 1;
    return -1;
  }
  // edge_ == RIGHT_EDGE.
  if (x > reference_x)
    return 1;
  return -1;
}

void SnapSizer::ChangeBounds(int x, int delta) {
  int index = std::min(static_cast<int>(arraysize(kSizes)) - 1,
                       std::max(size_index_ + delta, 0));
  if (index != size_index_) {
    size_index_ = index;
    target_bounds_ = GetTargetBounds();
  }
  num_moves_since_adjust_ = 0;
  last_adjust_x_ = x;
}

gfx::Rect SnapSizer::GetTargetBounds() const {
  return GetTargetBoundsForSize(size_index_);
}

bool SnapSizer::AlongEdge(int x) const {
  gfx::Rect area(ScreenAsh::GetDisplayBoundsInParent(window_));
  return (x <= area.x()) || (x >= area.right() - 1);
}

}  // namespace internal
}  // namespace ash

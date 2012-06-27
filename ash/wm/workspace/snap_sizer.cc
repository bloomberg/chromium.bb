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

// Percent of the screen (width) to give the window.
const float kPercents[] = { .5f, 2.0f / 3.0f, .8f };

// Windows are initially snapped to the percent at index 0. The index into
// |kPercents| is changed if any of the following happen:
// . The user stops moving the mouse for |kDelayBeforeIncreaseMS| and then moves
//   the mouse again.
// . The mouse moves |kPixelsBeforeAdjust| horizontal pixels.
// . The mouse is against the edge of the screen and the mouse is moved
//   |kMovesBeforeAdjust| times.
const int kDelayBeforeIncreaseMS = 500;
const int kMovesBeforeAdjust = 50;
const int kPixelsBeforeAdjust = 100;

}  // namespace

SnapSizer::SnapSizer(aura::Window* window,
                     const gfx::Point& start,
                     Edge edge,
                     int grid_size)
    : window_(window),
      edge_(edge),
      grid_size_(grid_size),
      time_last_update_(base::TimeTicks::Now()),
      percent_index_(0),
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
  size_t current;
  for (current = 0; current < arraysize(kPercents); ++current) {
    gfx::Rect target = GetTargetBoundsForPercent(current);
    if (target == bounds) {
      ++current;
      break;
    }
  }
  return GetTargetBoundsForPercent(current % arraysize(kPercents));
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
  int index = std::min(static_cast<int>(arraysize(kPercents)) - 1,
                       std::max(percent_index_ + delta, 0));
  if (index != percent_index_) {
    percent_index_ = index;
    target_bounds_ = GetTargetBounds();
  }
  num_moves_since_adjust_ = 0;
  last_adjust_x_ = x;
}

gfx::Rect SnapSizer::GetTargetBounds() const {
  return GetTargetBoundsForPercent(percent_index_);
}

gfx::Rect SnapSizer::GetTargetBoundsForPercent(int percent_index) const {
  gfx::Rect work_area(ScreenAsh::GetUnmaximizedWorkAreaBounds(window_));
  int y = WindowResizer::AlignToGridRoundUp(work_area.y(), grid_size_);
  // We don't align to the bottom of the grid as the launcher may not
  // necessarily align to the grid (happens when auto-hidden).
  int max_y = work_area.bottom();
  int width = static_cast<float>(work_area.width()) * kPercents[percent_index];
  if (edge_ == LEFT_EDGE) {
    int x = WindowResizer::AlignToGridRoundUp(work_area.x(), grid_size_);
    int mid_x = WindowResizer::AlignToGridRoundUp(
        work_area.x() + width, grid_size_);
    return gfx::Rect(x, y, mid_x - x, max_y - y);
  }
  int max_x =
      WindowResizer::AlignToGridRoundDown(work_area.right(), grid_size_);
  int x = WindowResizer::AlignToGridRoundUp(max_x - width, grid_size_);
  return gfx::Rect(x , y, max_x - x, max_y - y);
}

bool SnapSizer::AlongEdge(int x) const {
  // TODO: need to support multi-display.
  gfx::Rect area(gfx::Screen::GetDisplayNearestWindow(window_).bounds());
  return (x <= area.x()) || (x >= area.right() - 1);
}

}  // namespace internal
}  // namespace ash

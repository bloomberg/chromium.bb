// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/snap_sizer.h"

#include <cmath>

#include "ash/screen_ash.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// A list of ideal window width in pixel which will be used to populate the
// |usable_width_| list.
const int kIdealWidth[] = { 1280, 1024, 768, 640 };

// Windows are initially snapped to the size in |usable_width_| at index 0.
// The index into |usable_width_| is changed if any of the following happen:
// . The user stops moving the mouse for |kDelayBeforeIncreaseMS| and then
//   moves the mouse again.
// . The mouse moves |kPixelsBeforeAdjust| horizontal pixels.
// . The mouse is against the edge of the screen and the mouse is moved
//   |kMovesBeforeAdjust| times.
const int kDelayBeforeIncreaseMS = 500;
const int kMovesBeforeAdjust = 25;
const int kPixelsBeforeAdjust = 100;

// When the smallest resolution does not fit on the screen, we take this
// fraction of the available space.
const int kMinimumScreenPercent = 90;

// Create the list of possible width for the current screen configuration:
// Fill the |usable_width_| list with items from |kIdealWidth| which fit on
// the screen and supplement it with the 'half of screen' size. Furthermore,
// add an entry for 90% of the screen size if it is smaller then the biggest
// value in the |kIdealWidth| list (to get a step between the values).
std::vector<int> BuildIdealWidthList(aura::Window* window) {
  std::vector<int> ideal_width_list;
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(window));
  int half_size = work_area.width() / 2;
  int maximum_width = (kMinimumScreenPercent * work_area.width()) / 100;
  for (size_t i = 0; i < arraysize(kIdealWidth); i++) {
    if (maximum_width >= kIdealWidth[i]) {
      if (i && !ideal_width_list.size() && maximum_width != kIdealWidth[i])
        ideal_width_list.push_back(maximum_width);
      if (half_size > kIdealWidth[i])
        ideal_width_list.push_back(half_size);
      if (half_size >= kIdealWidth[i])
        half_size = 0;
      ideal_width_list.push_back(kIdealWidth[i]);
    }
  }
  if (half_size)
    ideal_width_list.push_back(half_size);

  return ideal_width_list;
}

}  // namespace

SnapSizer::SnapSizer(aura::Window* window,
                     const gfx::Point& start,
                     Edge edge,
                     InputType input_type)
    : window_(window),
      edge_(edge),
      time_last_update_(base::TimeTicks::Now()),
      size_index_(0),
      resize_disabled_(false),
      num_moves_since_adjust_(0),
      last_adjust_x_(start.x()),
      last_update_x_(start.x()),
      start_x_(start.x()),
      input_type_(input_type),
      usable_width_(BuildIdealWidthList(window)) {
  DCHECK(!usable_width_.empty());
  target_bounds_ = GetTargetBounds();
}

SnapSizer::~SnapSizer() {
}

void SnapSizer::SnapWindow(aura::Window* window, SnapSizer::Edge edge) {
  if (!wm::CanSnapWindow(window))
    return;
  internal::SnapSizer sizer(window, gfx::Point(), edge,
      internal::SnapSizer::OTHER_INPUT);
  if (wm::IsWindowFullscreen(window) || wm::IsWindowMaximized(window)) {
    // Before we can set the bounds we need to restore the window.
    // Restoring the window will set the window to its restored bounds.
    // To avoid an unnecessary bounds changes (which may have side effects)
    // we set the restore bounds to the bounds we want, restore the window,
    // then reset the restore bounds. This way no unnecessary bounds
    // changes occurs and the original restore bounds is remembered.
    gfx::Rect restore = *GetRestoreBoundsInScreen(window);
    SetRestoreBoundsInParent(window, sizer.GetSnapBounds(window->bounds()));
    wm::RestoreWindow(window);
    SetRestoreBoundsInScreen(window, restore);
  } else {
    window->SetBounds(sizer.GetSnapBounds(window->bounds()));
  }
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
    int pixels_before_adjust = kPixelsBeforeAdjust;
    if (input_type_ == TOUCH_MAXIMIZE_BUTTON_INPUT) {
      const gfx::Rect& workspace_bounds = window_->parent()->bounds();
      if (start_x_ > location.x()) {
        pixels_before_adjust =
            std::min(pixels_before_adjust, start_x_ / 10);
      } else {
        pixels_before_adjust =
            std::min(pixels_before_adjust,
                     (workspace_bounds.width() - start_x_) / 10);
      }
    }
    if (std::abs(location.x() - last_adjust_x_) >= pixels_before_adjust ||
        (along_edge && num_moves_since_adjust_ >= kMovesBeforeAdjust)) {
      ChangeBounds(location.x(),
                   CalculateIncrement(location.x(), last_adjust_x_));
    }
  }
  last_update_x_ = location.x();
  time_last_update_ = base::TimeTicks::Now();
}

gfx::Rect SnapSizer::GetSnapBounds(const gfx::Rect& bounds) {
  int current = 0;
  if (!resize_disabled_) {
    for (current = usable_width_.size() - 1; current >= 0; current--) {
      gfx::Rect target = GetTargetBoundsForSize(current);
      if (target == bounds) {
        ++current;
        break;
      }
    }
  }
  return GetTargetBoundsForSize(current % usable_width_.size());
}

void SnapSizer::SelectDefaultSizeAndDisableResize() {
  resize_disabled_ = true;
  size_index_ = 0;
  target_bounds_ = GetTargetBounds();
}

gfx::Rect SnapSizer::GetTargetBoundsForSize(size_t size_index) const {
  gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(window_));
  int y = work_area.y();
  // We don't align to the bottom of the grid as the launcher may not
  // necessarily align to the grid (happens when auto-hidden).
  int max_y = work_area.bottom();
  int width = 0;
  if (resize_disabled_) {
    // Make sure that we keep the size of the window smaller then a certain
    // fraction of the screen space.
    int minimum_size = (kMinimumScreenPercent * work_area.width()) / 100;
    width = std::max(std::min(minimum_size, 1024), work_area.width() / 2);
  } else {
    DCHECK(size_index < usable_width_.size());
    width = usable_width_[size_index];
  }

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
  int index = std::min(static_cast<int>(usable_width_.size()) - 1,
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

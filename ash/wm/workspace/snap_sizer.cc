// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/snap_sizer.h"

#include <cmath>

#include "ash/ash_switches.h"
#include "ash/screen_util.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

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

// Returns the minimum width that |window| can be snapped to.
int GetMinWidth(aura::Window* window) {
  return window->delegate() ? window->delegate()->GetMinimumSize().width() : 0;
}

// Returns the width that |window| should be snapped to if resizing is disabled
// in the SnapSizer.
int GetDefaultWidth(aura::Window* window) {
  gfx::Rect work_area(ScreenUtil::GetDisplayWorkAreaBoundsInParent(window));
  return std::max(work_area.width() / 2, GetMinWidth(window));
}

// Changes |window|'s bounds to |snap_bounds| while preserving the restore
// bounds.
void SnapWindowToBounds(wm::WindowState* window_state,
                        SnapSizer::Edge edge,
                        const gfx::Rect& snap_bounds) {
  if (edge == SnapSizer::LEFT_EDGE) {
    window_state->SnapLeft(snap_bounds);
  } else {
    window_state->SnapRight(snap_bounds);
  }
}

}  // namespace

// static
const int SnapSizer::kScreenEdgeInsetForTouchDrag = 32;

SnapSizer::SnapSizer(wm::WindowState* window_state,
                     const gfx::Point& start,
                     Edge edge,
                     InputType input_type)
    : window_state_(window_state),
      edge_(edge),
      time_last_update_(base::TimeTicks::Now()),
      end_of_sequence_(false),
      num_moves_since_adjust_(0),
      last_adjust_x_(start.x()),
      last_update_x_(start.x()),
      start_x_(start.x()),
      input_type_(input_type) {
  target_bounds_ = CalculateTargetBounds();
}

SnapSizer::~SnapSizer() {
}

void SnapSizer::SnapWindow(wm::WindowState* window_state,
                           SnapSizer::Edge edge) {
  if (!window_state->CanSnap())
    return;
  internal::SnapSizer sizer(window_state, gfx::Point(), edge,
      internal::SnapSizer::OTHER_INPUT);
  sizer.SnapWindowToTargetBounds();
}

void SnapSizer::SnapWindowToTargetBounds() {
  SnapWindowToBounds(window_state_, edge_, target_bounds());
}

void SnapSizer::Update(const gfx::Point& location) {
  // See description above for details on this behavior.
  num_moves_since_adjust_++;
  if ((base::TimeTicks::Now() - time_last_update_).InMilliseconds() >
      kDelayBeforeIncreaseMS) {
    CheckEndOfSequence(location.x(), last_update_x_);
  } else {
    bool along_edge = AlongEdge(location.x());
    if (std::abs(location.x() - last_adjust_x_) >= kPixelsBeforeAdjust ||
        (along_edge && num_moves_since_adjust_ >= kMovesBeforeAdjust)) {
      CheckEndOfSequence(location.x(), last_adjust_x_);
    }
  }
  last_update_x_ = location.x();
  time_last_update_ = base::TimeTicks::Now();
}

gfx::Rect SnapSizer::CalculateTargetBounds() const {
  gfx::Rect work_area(ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      window_state_->window()));
  int y = work_area.y();
  int max_y = work_area.bottom();
  int width = GetDefaultWidth(window_state_->window());

  if (edge_ == LEFT_EDGE) {
    int x = work_area.x();
    int mid_x = x + width;
    return gfx::Rect(x, y, mid_x - x, max_y - y);
  }
  int max_x = work_area.right();
  int x = max_x - width;
  return gfx::Rect(x , y, max_x - x, max_y - y);
}

void SnapSizer::CheckEndOfSequence(int x, int reference_x) {
  end_of_sequence_ = AlongEdge(x) ||
      (edge_ == LEFT_EDGE ? (x < reference_x) : (x > reference_x));
  num_moves_since_adjust_ = 0;
  last_adjust_x_ = x;
}

bool SnapSizer::AlongEdge(int x) const {
  gfx::Rect area(ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      window_state_->window()));
  int inset =
      input_type_ == TOUCH_DRAG_INPUT ? kScreenEdgeInsetForTouchDrag : 0;
  return (x <= area.x() + inset) || (x >= area.right() - 1 - inset);
}

}  // namespace internal
}  // namespace ash

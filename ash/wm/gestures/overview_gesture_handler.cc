// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/overview_gesture_handler.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {
namespace {

// The threshold before engaging overview on a three finger swipe on the
// touchpad.
const float kSwipeThresholdPixels = 300;

}  // namespace

OverviewGestureHandler::OverviewGestureHandler() : scroll_x_(0), scroll_y_(0) {}

OverviewGestureHandler::~OverviewGestureHandler() {
}

bool OverviewGestureHandler::ProcessScrollEvent(const ui::ScrollEvent& event) {
  if (event.type() == ui::ET_SCROLL_FLING_START ||
      event.type() == ui::ET_SCROLL_FLING_CANCEL ||
      event.finger_count() != 3) {
    scroll_x_ = scroll_y_ = 0;
    return false;
  }

  scroll_x_ += event.x_offset();
  scroll_y_ += event.y_offset();

  // Horizontal swiping is ignored.
  if (std::fabs(scroll_x_) >= std::fabs(scroll_y_)) {
    scroll_x_ = scroll_y_ = 0;
    return false;
  }

  // Only allow swipe up to enter overview, down to exit. Ignore extra swiping
  // in the wrong direction.
  Shell* shell = Shell::GetInstance();
  if (shell->window_selector_controller()->IsSelecting()) {
    if (scroll_y_ < 0)
      scroll_x_ = scroll_y_ = 0;
    if (scroll_y_ < kSwipeThresholdPixels)
      return false;
  } else {
    if (scroll_y_ > 0)
      scroll_x_ = scroll_y_ = 0;
    if (scroll_y_ > -kSwipeThresholdPixels)
      return false;
  }

  // Reset scroll amount on toggling.
  scroll_x_ = scroll_y_ = 0;
  shell->metrics()->RecordUserMetricsAction(UMA_TOUCHPAD_GESTURE_OVERVIEW);
  shell->window_selector_controller()->ToggleOverview();
  return true;
}

}  // namespace ash

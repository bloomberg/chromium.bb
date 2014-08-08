// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/overview_gesture_handler.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// The number of pixels from the top of the screen (a virtual bezel) to allow
// the overview swipe down gesture to begin within. Note this does not actually
// prevent handling of the touches so a fullscreen page which consumes these can
// prevent entering overview with a swipe down.
const int kTopBezelExtraPixels = 5;

// The threshold before engaging overview on a three finger swipe on the
// touchpad.
const float kSwipeThresholdPixels = 300;

}  // namespace;

OverviewGestureHandler::OverviewGestureHandler()
    : in_top_bezel_gesture_(false),
      scroll_x_(0),
      scroll_y_(0) {
}

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

bool OverviewGestureHandler::ProcessGestureEvent(
    const ui::GestureEvent& event) {
  // Detect at the beginning of any gesture whether it begins on the top bezel.
  if (event.type() == ui::ET_GESTURE_BEGIN &&
      event.details().touch_points() == 1) {
    gfx::Point point_in_screen(event.location());
    aura::Window* target = static_cast<aura::Window*>(event.target());
    ::wm::ConvertPointToScreen(target, &point_in_screen);
    in_top_bezel_gesture_ = !Shell::GetScreen()->GetDisplayNearestPoint(
        point_in_screen).bounds().y() + kTopBezelExtraPixels >
            point_in_screen.y();
    return false;
  }

  if (!in_top_bezel_gesture_ ||
      event.type() != ui::ET_GESTURE_SWIPE ||
      !event.details().swipe_down() ||
      event.details().touch_points() != 3) {
    return false;
  }

  Shell* shell = Shell::GetInstance();
  shell->metrics()->RecordUserMetricsAction(UMA_GESTURE_OVERVIEW);
  shell->window_selector_controller()->ToggleOverview();
  return true;
}

}  // namespace ash

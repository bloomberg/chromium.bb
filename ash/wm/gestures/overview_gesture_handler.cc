// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/overview_gesture_handler.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// The number of pixels from the top of the screen (a virtual bezel) to allow
// the overview swipe down gesture to begin within. Note this does not actually
// prevent handling of the touches so a fullscreen page which consumes these can
// prevent entering overview with a swipe down.
const int kTopBezelExtraPixels = 5;

}  // namespace;

OverviewGestureHandler::OverviewGestureHandler()
    : in_top_bezel_gesture_(false) {
}

OverviewGestureHandler::~OverviewGestureHandler() {
}

bool OverviewGestureHandler::ProcessGestureEvent(
    const ui::GestureEvent& event) {
  // Detect at the beginning of any gesture whether it begins on the top bezel.
  if (event.type() == ui::ET_GESTURE_BEGIN &&
      event.details().touch_points() == 1) {
    in_top_bezel_gesture_ = !Shell::GetScreen()->GetDisplayNearestPoint(
        event.location()).bounds().y() + kTopBezelExtraPixels >
            event.location().y();
    return false;
  }

  if (!in_top_bezel_gesture_ ||
      event.type() != ui::ET_GESTURE_MULTIFINGER_SWIPE ||
      !event.details().swipe_down() ||
      event.details().touch_points() != 3) {
    return false;
  }

  Shell* shell = Shell::GetInstance();
  shell->delegate()->RecordUserMetricsAction(UMA_GESTURE_OVERVIEW);
  shell->window_selector_controller()->ToggleOverview();
  return true;
}

}  // namespace internal
}  // namespace ash

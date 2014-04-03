// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_

#include "base/basictypes.h"

namespace aura {
class Window;
}

namespace ui {
class GestureEvent;
class ScrollEvent;
}

namespace ash {

// This handles a 3-finger swipe down gesture from the top of the screen to
// enter overview mode.
class OverviewGestureHandler {
 public:
  OverviewGestureHandler();
  virtual ~OverviewGestureHandler();

  // Processes a scroll event and may start overview. Returns true if the event
  // has been handled and should not be processed further, false otherwise.
  bool ProcessScrollEvent(const ui::ScrollEvent& event);

  // Processes a gesture event and may start overview. Returns true if the event
  // has been handled and should not be processed any farther, false otherwise.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

 private:
  // True if the current/last gesture began in the top bezel.
  bool in_top_bezel_gesture_;

  // The total distance scrolled with three fingers.
  float scroll_x_;
  float scroll_y_;

  DISALLOW_COPY_AND_ASSIGN(OverviewGestureHandler);
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_

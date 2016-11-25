// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_

#include "base/macros.h"

namespace ui {
class ScrollEvent;
}

namespace ash {

// This handles 3-finger touchpad scroll events to enter/exit overview mode.
class OverviewGestureHandler {
 public:
  OverviewGestureHandler();
  virtual ~OverviewGestureHandler();

  // Processes a scroll event and may start overview. Returns true if the event
  // has been handled and should not be processed further, false otherwise.
  bool ProcessScrollEvent(const ui::ScrollEvent& event);

 private:
  // The total distance scrolled with three fingers.
  float scroll_x_;
  float scroll_y_;

  DISALLOW_COPY_AND_ASSIGN(OverviewGestureHandler);
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_

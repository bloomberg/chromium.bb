// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_
#define ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/events/event_handler.h"

namespace ash {
class LongPressAffordanceHandler;
class OverviewGestureHandler;
class ShelfGestureHandler;

namespace test {
class SystemGestureEventFilterTest;
}

// An event filter which handles system level gesture events.
class SystemGestureEventFilter : public ui::EventHandler {
 public:
  SystemGestureEventFilter();
  virtual ~SystemGestureEventFilter();

  // Overridden from ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  friend class ash::test::SystemGestureEventFilterTest;

  scoped_ptr<LongPressAffordanceHandler> long_press_affordance_;
  scoped_ptr<OverviewGestureHandler> overview_gesture_handler_;
  scoped_ptr<ShelfGestureHandler> shelf_gesture_handler_;

  DISALLOW_COPY_AND_ASSIGN(SystemGestureEventFilter);
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_GESTURE_EVENT_FILTER_H_

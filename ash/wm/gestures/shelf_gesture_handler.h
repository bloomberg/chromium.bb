// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_SHELF_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_SHELF_GESTURE_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ash {

// This manages gestures on the shelf (e.g. launcher, status tray) that affects
// the shelf visibility.
class ShelfGestureHandler {
 public:
  ShelfGestureHandler();
  virtual ~ShelfGestureHandler();

  // Processes a gesture event and updates the status of the shelf when
  // appropriate. Returns true of the gesture has been handled and it should not
  // be processed any farther, false otherwise.
  // The caller must provide the |event_target_window| because the caller has
  // knowledge of the type of the |event.target()| whether it's a |views::View|
  // or an |aura::Window|.
  bool ProcessGestureEvent(const ui::GestureEvent& event,
                           const aura::Window* event_target_window);

 private:
  bool drag_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(ShelfGestureHandler);
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_SHELF_GESTURE_HANDLER_H_

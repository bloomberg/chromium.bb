// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_TRAY_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_TRAY_GESTURE_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class GestureEvent;
}

namespace ash {
namespace internal {

// Handles gesture events on the shelf to show the system tray bubble.
class TrayGestureHandler : public views::WidgetObserver {
 public:
  TrayGestureHandler();
  virtual ~TrayGestureHandler();

  // Handles a gesture-update event and updates the dragging state of the tray
  // bubble. Returns true if the handler can continue to process gesture events
  // for the bubble. Returns false if it should no longer receive gesture
  // events.
  bool UpdateGestureDrag(const ui::GestureEvent& event);

  void CompleteGestureDrag(const ui::GestureEvent& event);

 private:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // The widget for the tray-bubble.
  views::Widget* widget_;

  // The amount that has been dragged.
  float gesture_drag_amount_;

  DISALLOW_COPY_AND_ASSIGN(TrayGestureHandler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_GESTURES_TRAY_GESTURE_HANDLER_H_

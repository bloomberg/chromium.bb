// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_TWO_FINGER_DRAG_HANDLER_H_
#define ASH_WM_GESTURES_TWO_FINGER_DRAG_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ui {
class GestureEvent;
}

namespace ash {

class WindowResizer;

namespace internal {

// This handles 2-finger drag gestures to move toplevel windows.
class TwoFingerDragHandler : public aura::WindowObserver {
 public:
  TwoFingerDragHandler();
  virtual ~TwoFingerDragHandler();

  // Processes a gesture event and starts a drag, or updates/ends an in-progress
  // drag. Returns true if the event has been handled and should not be
  // processed any farther, false otherwise.
  bool ProcessGestureEvent(aura::Window* target, const ui::GestureEvent& event);

 private:
  void Reset();

  // Overridden from aura::WindowObserver.
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  int first_finger_hittest_;

  scoped_ptr<WindowResizer> window_resizer_;

  DISALLOW_COPY_AND_ASSIGN(TwoFingerDragHandler);
};

}
}  // namespace ash

#endif  // ASH_WM_GESTURES_TWO_FINGER_DRAG_HANDLER_H_

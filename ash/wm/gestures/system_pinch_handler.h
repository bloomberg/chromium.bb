// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_SYSTEM_PINCH_HANDLER_H_
#define ASH_WM_GESTURES_SYSTEM_PINCH_HANDLER_H_

#include "ash/wm/workspace/phantom_window_controller.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ui {
class GestureEvent;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

enum SystemGestureStatus {
  SYSTEM_GESTURE_PROCESSED,  // The system gesture has been processed.
  SYSTEM_GESTURE_IGNORED,    // The system gesture was ignored.
  SYSTEM_GESTURE_END,        // Marks the end of the sytem gesture.
};

// This handles 4+ finger pinch gestures to maximize/minimize/restore windows.
class SystemPinchHandler {
 public:
  explicit SystemPinchHandler(aura::Window* target);
  virtual ~SystemPinchHandler();

  // Processes a gesture event. Returns SYSTEM_GESTURE_PROCESSED if the gesture
  // event has been processed. Returns SYSTEM_GESTURE_END if the gesture event
  // has been processed, and marks the end of the gesture sequence (i.e. the
  // handler should receive no more input events).
  SystemGestureStatus ProcessGestureEvent(const ui::GestureEvent& event);

  static const int kSystemGesturePoints;

 private:
  // Returns the appropriate bounds for the phantom window depending on the
  // state of the window, the state of the gesture sequence, and the current
  // event location.
  gfx::Rect GetPhantomWindowScreenBounds(aura::Window* window,
                                         const gfx::Point& point);

  enum PhantomWindowState {
    PHANTOM_WINDOW_NORMAL,
    PHANTOM_WINDOW_MAXIMIZED,
    PHANTOM_WINDOW_MINIMIZED,
  };

  aura::Window* target_;
  views::Widget* widget_;

  // A phantom window is used to provide visual cues for
  // pinch-to-resize/maximize/minimize gestures.
  scoped_ptr<PhantomWindowController> phantom_;

  // When the phantom window is in minimized or maximized state, moving the
  // target window should not move the phantom window. So |phantom_state_| is
  // used to track the state of the phantom window.
  PhantomWindowState phantom_state_;

  // PINCH_UPDATE events include incremental pinch-amount. But it is necessary
  // to keep track of the overall pinch-amount. |pinch_factor_| is used for
  // that.
  double pinch_factor_;

  DISALLOW_COPY_AND_ASSIGN(SystemPinchHandler);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_GESTURES_SYSTEM_PINCH_HANDLER_H_

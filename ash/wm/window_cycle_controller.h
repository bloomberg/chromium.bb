// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_CONTROLLER_H_
#define ASH_WM_WINDOW_CYCLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace ui {
class EventHandler;
}

namespace ash {

class WindowCycleList;

// Controls cycling through windows with the keyboard, for example, via alt-tab.
// Windows are sorted primarily by most recently used, and then by screen order.
// We activate windows as you cycle through them, so the order on the screen
// may change during the gesture, but the most recently used list isn't updated
// until the cycling ends.  Thus we maintain the state of the windows
// at the beginning of the gesture so you can cycle through in a consistent
// order.
class ASH_EXPORT WindowCycleController {
 public:
  enum Direction {
    FORWARD,
    BACKWARD
  };
  WindowCycleController();
  virtual ~WindowCycleController();

  // Returns true if cycling through windows is enabled. This is false at
  // certain times, such as when the lock screen is visible.
  static bool CanCycle();

  // Cycles between windows in the given |direction|. If |is_alt_down| then
  // interprets this call as the start of a multi-step cycle sequence and
  // installs a key filter to watch for alt being released.
  void HandleCycleWindow(Direction direction, bool is_alt_down);

  // Cycles between windows without maintaining a multi-step cycle sequence
  // (see above).
  void HandleLinearCycleWindow();

  // Informs the controller that the Alt key has been released and it can
  // terminate the existing multi-step cycle.
  void AltKeyReleased();

  // Returns true if we are in the middle of a window cycling gesture.
  bool IsCycling() const { return windows_.get() != NULL; }

  // Returns the WindowCycleList. Really only useful for testing.
  const WindowCycleList* windows() const { return windows_.get(); }

 private:
  // Call to start cycling windows.  You must call StopCycling() when done.
  void StartCycling();

  // Cycles to the next or previous window based on |direction|.
  void Step(Direction direction);

  // Installs an event filter to watch for release of the alt key.
  void InstallEventFilter();

  // Stops the current window cycle and cleans up the event filter.
  void StopCycling();

  scoped_ptr<WindowCycleList> windows_;

  // Event handler to watch for release of alt key.
  scoped_ptr<ui::EventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleController);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_CONTROLLER_H_

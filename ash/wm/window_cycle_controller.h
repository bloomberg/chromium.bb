// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_CONTROLLER_H_
#define ASH_WM_WINDOW_CYCLE_CONTROLLER_H_
#pragma once

#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
}

namespace ash {

class WindowCycleEventFilter;

// Controls cycling through windows with the keyboard, for example, via alt-tab.
// We activate windows as you cycle through them, so the order on the screen
// may change during the gesture.  Thus we maintain the state of the windows
// at the beginning of the gesture so you can cycle through in a consistent
// order.
class ASH_EXPORT WindowCycleController {
 public:
  enum Direction {
    FORWARD,
    BACKWARD
  };
  WindowCycleController();
  ~WindowCycleController();

  // Cycles between windows in the given |direction|. If |is_alt_down| then
  // interprets this call as the start of a multi-step cycle sequence and
  // installs a key filter to watch for alt being released.
  void HandleCycleWindow(Direction direction, bool is_alt_down);

  // Informs the controller that the Alt key has been released and it can
  // terminate the existing multi-step cycle.
  void AltKeyReleased();

  // Returns true if we are in the middle of a window cycling gesture.
  bool IsCycling() const { return current_index_ != -1; }

 private:
  typedef std::vector<aura::Window*> WindowList;

  // Call to start cycling windows.  You must call StopCycling() when done.
  void StartCycling();

  // Cycles to the next or previous window based on |direction|.
  void Step(Direction direction);

  // Installs an event filter to watch for release of the alt key.
  void InstallEventFilter();

  // Stops the current window cycle and cleans up the event filter.
  void StopCycling();

  // Returns the index of |window| in |windows_| or -1 if it isn't there.
  int GetWindowIndex(aura::Window* window);

  // List of weak pointers to windows to use while cycling with the keyboard.
  // List is built when the user initiates the gesture (e.g. hits alt-tab the
  // first time) and is emptied when the gesture is complete (e.g. releases the
  // alt key).
  WindowList windows_;

  // Current position in the |windows_| list or -1 if we're not cycling.
  int current_index_;

  // Event filter to watch for release of alt key.
  scoped_ptr<WindowCycleEventFilter> event_filter_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleController);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_CONTROLLER_H_

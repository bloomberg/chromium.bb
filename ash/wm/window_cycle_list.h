// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_LIST_H_
#define ASH_WM_WINDOW_CYCLE_LIST_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/window_cycle_controller.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"

namespace ash {

class ScopedShowWindow;

// Tracks a set of Windows that can be stepped through. This class is used by
// the WindowCycleController.
class ASH_EXPORT WindowCycleList : public aura::WindowObserver {
 public:
  typedef std::vector<aura::Window*> WindowList;

  explicit WindowCycleList(const WindowList& windows);
  virtual ~WindowCycleList();

  bool empty() const { return windows_.empty(); }

  // Cycles to the next or previous window based on |direction|.
  void Step(WindowCycleController::Direction direction);

  const WindowList& windows() const { return windows_; }

 private:
  // aura::WindowObserver overrides:
  // There is a chance a window is destroyed, for example by JS code. We need to
  // take care of that even if it is not intended for the user to close a window
  // while window cycling.
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // List of weak pointers to windows to use while cycling with the keyboard.
  // List is built when the user initiates the gesture (i.e. hits alt-tab the
  // first time) and is emptied when the gesture is complete (i.e. releases the
  // alt key).
  WindowList windows_;

  // Current position in the |windows_|
  int current_index_;

  // Wrapper for the window brought to the front.
  scoped_ptr<ScopedShowWindow> showing_window_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleList);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_LIST_H_

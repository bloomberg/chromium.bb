// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_LIST_H_
#define ASH_WM_WINDOW_CYCLE_LIST_H_
#pragma once

#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Tracks a set of Windows that can be stepped through. This class is used by
// the WindowCycleController.
class ASH_EXPORT WindowCycleList : public aura::WindowObserver {
 public:
  typedef std::vector<aura::Window*> WindowList;

  enum Direction {
    FORWARD,
    BACKWARD
  };

  explicit WindowCycleList(const WindowList& windows);
  virtual ~WindowCycleList();

  bool empty() const { return windows_.empty(); }

  // Cycles to the next or previous window based on |direction|.
  void Step(Direction direction);

  const WindowList& windows() const { return windows_; }

 private:
  // Returns the index of |window| in |windows_| or -1 if it isn't there.
  int GetWindowIndex(aura::Window* window);

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // List of weak pointers to windows to use while cycling with the keyboard.
  // List is built when the user initiates the gesture (e.g. hits alt-tab the
  // first time) and is emptied when the gesture is complete (e.g. releases the
  // alt key).
  WindowList windows_;

  // Current position in the |windows_| list or -1 if we're not cycling.
  int current_index_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleList);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_LIST_H_

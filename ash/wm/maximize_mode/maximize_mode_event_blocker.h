// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_BLOCKER_H_
#define ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_BLOCKER_H_

#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/scoped_window_targeter.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// A class which blocks mouse and keyboard events while instantiated by
// replacing the root window event targeter.
class MaximizeModeEventBlocker : public ShellObserver {
 public:
  MaximizeModeEventBlocker();
  virtual ~MaximizeModeEventBlocker();

  // ShellObserver:
  virtual void OnRootWindowAdded(aura::Window* root_window) OVERRIDE;

 private:
  // Adds an event targeter on |root_window| to block mouse and keyboard events.
  void AddEventTargeterOn(aura::Window* root_window);

  ScopedVector<aura::ScopedWindowTargeter> targeters_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeEventBlocker);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_EVENT_BLOCKER_H_

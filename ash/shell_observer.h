// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_OBSERVER_H_
#define ASH_SHELL_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/system/user/login_status.h"

namespace aura {
class Window;
}

namespace ash {

class ASH_EXPORT ShellObserver {
 public:
  // Invoked after the screen's work area insets changes.
  virtual void OnDisplayWorkAreaInsetsChanged() {}

  // Invoked when the user logs in.
  virtual void OnLoginStateChanged(user::LoginStatus status) {}

  // Invoked when the application is exiting.
  virtual void OnAppTerminating() {}

  // Invoked when the screen is locked (after the lock window is visible) or
  // unlocked.
  virtual void OnLockStateChanged(bool locked) {}

  // Invoked after a non-primary root window is created.
  virtual void OnRootWindowAdded(aura::Window* root_window) {}

  // Invoked after the shelf has been created for |root_window|.
  virtual void OnShelfCreatedForRootWindow(aura::Window* root_window) {}

  // Invoked when the shelf alignment in |root_window| is changed.
  virtual void OnShelfAlignmentChanged(aura::Window* root_window) {}

  // Invoked when the projection touch HUD is toggled.
  virtual void OnTouchHudProjectionToggled(bool enabled) {}

  // Invoked when entering or exiting fullscreen mode in |root_window|.
  virtual void OnFullscreenStateChanged(bool is_fullscreen,
                                        aura::Window* root_window) {}

  // Called when the overview mode is about to be started (before the windows
  // get re-arranged).
  virtual void OnOverviewModeStarting() {}

  // Called before the overview mode is ending (before the windows get arranged
  // to their final position).
  virtual void OnOverviewModeEnding() {}

  // Called when the always maximize mode has started. Windows might still
  // animate though.
  virtual void OnMaximizeModeStarted() {}

  // Called when the always maximize mode has ended. Windows may still be
  // animating but have been restored.
  virtual void OnMaximizeModeEnded() {}

 protected:
  virtual ~ShellObserver() {}
};

}  // namespace ash

#endif  // ASH_SHELL_OBSERVER_H_

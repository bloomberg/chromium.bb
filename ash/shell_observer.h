// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_OBSERVER_H_
#define ASH_SHELL_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/login_status.h"

namespace aura {
class Window;
}

namespace ash {

class WmWindow;

class ASH_EXPORT ShellObserver {
 public:
  // Called when a casting session is started or stopped.
  virtual void OnCastingSessionStartedOrStopped(bool started) {}

  // Invoked after a non-primary root window is created.
  virtual void OnRootWindowAdded(WmWindow* root_window) {}

  // Invoked after the shelf has been created for |root_window|.
  virtual void OnShelfCreatedForRootWindow(WmWindow* root_window) {}

  // Invoked when the shelf alignment in |root_window| is changed.
  virtual void OnShelfAlignmentChanged(WmWindow* root_window) {}

  // Invoked when the shelf auto-hide behavior in |root_window| is changed.
  virtual void OnShelfAutoHideBehaviorChanged(WmWindow* root_window) {}

  // Invoked when the projection touch HUD is toggled.
  virtual void OnTouchHudProjectionToggled(bool enabled) {}

  // Invoked when entering or exiting fullscreen mode in |root_window|.
  virtual void OnFullscreenStateChanged(bool is_fullscreen,
                                        WmWindow* root_window) {}

  // Invoked when |pinned_window| enter or exit pinned mode.
  virtual void OnPinnedStateChanged(WmWindow* pinned_window) {}

  // Called when the overview mode is about to be started (before the windows
  // get re-arranged).
  virtual void OnOverviewModeStarting() {}

  // Called after overview mode has ended.
  virtual void OnOverviewModeEnded() {}

  // Called when the always maximize mode has started. Windows might still
  // animate though.
  virtual void OnMaximizeModeStarted() {}

  // Called when the maximize mode is about to end.
  virtual void OnMaximizeModeEnding() {}

  // Called when the maximize mode has ended. Windows may still be
  // animating but have been restored.
  virtual void OnMaximizeModeEnded() {}

  // Called when keyboard is activated/deactivated in |root_window|.
  virtual void OnVirtualKeyboardStateChanged(bool activated,
                                             aura::Window* root_window) {}

  // Called at the end of Shell::Init.
  virtual void OnShellInitialized() {}

 protected:
  virtual ~ShellObserver() {}
};

}  // namespace ash

#endif  // ASH_SHELL_OBSERVER_H_

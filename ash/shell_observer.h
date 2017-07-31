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

class PrefService;

namespace ash {

class ASH_EXPORT ShellObserver {
 public:
  // Called when the AppList is shown or dismissed.
  virtual void OnAppListVisibilityChanged(bool shown,
                                          aura::Window* root_window) {}

  // Called when a casting session is started or stopped.
  virtual void OnCastingSessionStartedOrStopped(bool started) {}

  // Invoked after a non-primary root window is created.
  virtual void OnRootWindowAdded(aura::Window* root_window) {}

  // Invoked after the shelf has been created for |root_window|.
  virtual void OnShelfCreatedForRootWindow(aura::Window* root_window) {}

  // Invoked when the shelf alignment in |root_window| is changed.
  virtual void OnShelfAlignmentChanged(aura::Window* root_window) {}

  // Invoked when the shelf auto-hide behavior in |root_window| is changed.
  virtual void OnShelfAutoHideBehaviorChanged(aura::Window* root_window) {}

  // Invoked when the projection touch HUD is toggled.
  virtual void OnTouchHudProjectionToggled(bool enabled) {}

  // Invoked when entering or exiting fullscreen mode in |root_window|.
  virtual void OnFullscreenStateChanged(bool is_fullscreen,
                                        aura::Window* root_window) {}

  // Invoked when |pinned_window| enter or exit pinned mode.
  virtual void OnPinnedStateChanged(aura::Window* pinned_window) {}

  // Called when the overview mode is about to be started (before the windows
  // get re-arranged).
  virtual void OnOverviewModeStarting() {}

  // Called after overview mode has ended.
  virtual void OnOverviewModeEnded() {}

  // Called when the split view mode is about to be started (before the window
  // gets snapped and activated).
  virtual void OnSplitViewModeStarting() {}

  // Called after split view mode has ended.
  virtual void OnSplitViewModeEnded() {}

  // Called when keyboard is activated/deactivated in |root_window|.
  virtual void OnVirtualKeyboardStateChanged(bool activated,
                                             aura::Window* root_window) {}

  // Called when a new KeyboardController is created.
  virtual void OnKeyboardControllerCreated() {}

  // Called when voice interaction session starts / finishes.
  virtual void OnVoiceInteractionStatusChanged(bool running) {}

  // Called at the end of Shell::Init.
  virtual void OnShellInitialized() {}

  // Called near the end of ~Shell. Shell::Get() still returns the Shell, but
  // most of Shell's state has been deleted.
  virtual void OnShellDestroyed() {}

  // Called when the user profile pref service is available. Also called after
  // multiprofile user switch. Never called with the login screen profile.
  // On mash will be called with null at the start of user switch then again
  // with a pref service once the connection to the mojo pref service is made.
  // TODO(jamescook): Either maintain pref service connections for all multiuser
  // profiles or make the pref service switch atomic with active user switch.
  // http://crbug.com/705347
  virtual void OnActiveUserPrefServiceChanged(PrefService* pref_service) {}

 protected:
  virtual ~ShellObserver() {}
};

}  // namespace ash

#endif  // ASH_SHELL_OBSERVER_H_

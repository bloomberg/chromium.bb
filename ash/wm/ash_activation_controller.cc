// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_activation_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ui/views/corewm/window_modality_controller.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// AshActivationController, public:

AshActivationController::AshActivationController() {
}

AshActivationController::~AshActivationController() {
}

////////////////////////////////////////////////////////////////////////////////
// AshActivationController, ActivationControllerDelegate implementation:

aura::Window* AshActivationController::WillActivateWindow(
    aura::Window* window) {
  aura::Window* window_modal_transient =
      views::corewm::GetModalTransient(window);
  if (window_modal_transient)
    return window_modal_transient;

  // Fallback to launcher
  if (!window)
    window = PrepareToActivateLauncher();

  // Make sure the workspace manager switches to the workspace for window.
  // Without this CanReceiveEvents() below returns false and activation never
  // changes. CanReceiveEvents() returns false if |window| isn't in the active
  // workspace, in which case its parent is not visible.
  // TODO(sky): if I instead change the opacity of the parent this isn't an
  // issue, but will make animations trickier... Consider which one is better.
  if (window) {
    internal::RootWindowController* root_window_controller =
        GetRootWindowController(window->GetRootWindow());
    root_window_controller->workspace_controller()->
        SetActiveWorkspaceByWindow(window);
  }

  // Restore minimized window. This needs to be done before CanReceiveEvents()
  // is called as that function checks window visibility.
  if (window && wm::IsWindowMinimized(window))
    window->Show();

  // If the screen is locked, just bring the window to top so that
  // it will be activated when the lock window is destroyed.
  // TODO(beng): Call EventClient directly here, rather than conflating with
  //             window visibility.
  if (window && !window->CanReceiveEvents())
    return NULL;

  // TODO(beng): could probably move to being a ActivationChangeObserver on
  //             Shell.
  if (window) {
    DCHECK(window->GetRootWindow());
    Shell::GetInstance()->set_active_root_window(window->GetRootWindow());
  }
  return window;
}

aura::Window* AshActivationController::WillFocusWindow(
    aura::Window* window) {
  aura::Window* window_modal_transient =
      views::corewm::GetModalTransient(window);
  if (window_modal_transient)
    return window_modal_transient;
  return window;
}

aura::Window* AshActivationController::PrepareToActivateLauncher() {
  // If workspace controller is not available, then it means that the root
  // window is being destroyed. We can't activate any window then.
  if (!GetRootWindowController(
      Shell::GetActiveRootWindow())->workspace_controller()) {
    return NULL;
  }
  // Fallback to a launcher only when Spoken feedback is enabled.
  if (!Shell::GetInstance()->delegate()->IsSpokenFeedbackEnabled())
    return NULL;
  ShelfWidget* shelf = GetRootWindowController(
      Shell::IsLauncherPerDisplayEnabled() ?
          Shell::GetActiveRootWindow() :
          Shell::GetPrimaryRootWindow())->shelf();
  // Launcher's window may be already destroyed in shutting down process.
  if (!shelf)
    return NULL;
  // Notify launcher to allow activation via CanActivate().
  shelf->WillActivateAsFallback();
  return shelf->GetNativeWindow();
}

}  // namespace internal
}  // namespace ash

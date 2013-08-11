// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_selector_controller.h"

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_selector.h"
#include "ash/wm/window_util.h"

namespace ash {

WindowSelectorController::WindowSelectorController() {
}

WindowSelectorController::~WindowSelectorController() {
}

// static
bool WindowSelectorController::CanSelect() {
  // Don't allow a window overview if the screen is locked or a modal dialog is
  // open.
  return !Shell::GetInstance()->session_state_delegate()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen();
}

void WindowSelectorController::ToggleOverview() {
  if (window_selector_.get()) {
    window_selector_.reset();
  } else {
    std::vector<aura::Window*> windows = ash::Shell::GetInstance()->
        mru_window_tracker()->BuildMruWindowList();
    // Don't enter overview mode with no windows.
    if (windows.empty())
      return;

    // Deactivating the window will hide popup windows like the omnibar or
    // open menus.
    aura::Window* active_window = wm::GetActiveWindow();
    if (active_window)
      wm::DeactivateWindow(active_window);
    window_selector_.reset(new WindowSelector(windows, this));
  }
}

bool WindowSelectorController::IsSelecting() {
  return window_selector_.get() != NULL;
}

void WindowSelectorController::OnWindowSelected(aura::Window* window) {
  window_selector_.reset();
  wm::ActivateWindow(window);
}

void WindowSelectorController::OnSelectionCanceled() {
  window_selector_.reset();
}

}  // namespace ash

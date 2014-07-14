// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram.h"
#include "ui/aura/window.h"

namespace ash {

WindowSelectorController::WindowSelectorController() {
}

WindowSelectorController::~WindowSelectorController() {
}

// static
bool WindowSelectorController::CanSelect() {
  // Don't allow a window overview if the screen is locked or a modal dialog is
  // open or running in kiosk app session.
  return Shell::GetInstance()->session_state_delegate()->
             IsActiveUserSessionStarted() &&
         !Shell::GetInstance()->session_state_delegate()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen() &&
         Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus() !=
             user::LOGGED_IN_KIOSK_APP;
}

void WindowSelectorController::ToggleOverview() {
  if (IsSelecting()) {
    OnSelectionEnded();
  } else {
    // Don't start overview if window selection is not allowed.
    if (!CanSelect())
      return;

    std::vector<aura::Window*> windows = ash::Shell::GetInstance()->
        mru_window_tracker()->BuildMruWindowList();
    // Don't enter overview mode with no windows.
    if (windows.empty())
      return;

    window_selector_.reset(new WindowSelector(windows, this));
    OnSelectionStarted();
  }
}

bool WindowSelectorController::IsSelecting() {
  return window_selector_.get() != NULL;
}

// TODO(flackr): Make WindowSelectorController observe the activation of
// windows, so we can remove WindowSelectorDelegate.
void WindowSelectorController::OnSelectionEnded() {
  window_selector_.reset();
  last_selection_time_ = base::Time::Now();
}

void WindowSelectorController::OnSelectionStarted() {
  if (!last_selection_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Ash.WindowSelector.TimeBetweenUse",
        base::Time::Now() - last_selection_time_);
  }
}

}  // namespace ash

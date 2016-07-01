// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/window_selector_controller.h"

#include <vector>

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/metrics/histogram.h"

namespace ash {

WindowSelectorController::WindowSelectorController() {}

WindowSelectorController::~WindowSelectorController() {}

// static
bool WindowSelectorController::CanSelect() {
  // Don't allow a window overview if the screen is locked or a modal dialog is
  // open or running in kiosk app session.
  SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();
  return session_state_delegate->IsActiveUserSessionStarted() &&
         !session_state_delegate->IsScreenLocked() &&
         !WmShell::Get()->IsSystemModalWindowOpen() &&
         !WmShell::Get()->IsPinned() &&
         WmShell::Get()->system_tray_delegate()->GetUserLoginStatus() !=
             LoginStatus::KIOSK_APP;
}

void WindowSelectorController::ToggleOverview() {
  if (IsSelecting()) {
    OnSelectionEnded();
  } else {
    // Don't start overview if window selection is not allowed.
    if (!CanSelect())
      return;

    std::vector<WmWindow*> windows =
        WmShell::Get()->mru_window_tracker()->BuildMruWindowList();
    auto end =
        std::remove_if(windows.begin(), windows.end(),
                       std::not1(std::ptr_fun(&WindowSelector::IsSelectable)));
    windows.resize(end - windows.begin());

    // Don't enter overview mode with no windows.
    if (windows.empty())
      return;

    WmShell::Get()->OnOverviewModeStarting();
    window_selector_.reset(new WindowSelector(this));
    window_selector_->Init(windows);
    OnSelectionStarted();
  }
}

bool WindowSelectorController::IsSelecting() {
  return window_selector_.get() != NULL;
}

bool WindowSelectorController::IsRestoringMinimizedWindows() const {
  return window_selector_.get() != NULL &&
         window_selector_->restoring_minimized_windows();
}

// TODO(flackr): Make WindowSelectorController observe the activation of
// windows, so we can remove WindowSelectorDelegate.
void WindowSelectorController::OnSelectionEnded() {
  window_selector_->Shutdown();
  window_selector_.reset();
  last_selection_time_ = base::Time::Now();
  WmShell::Get()->OnOverviewModeEnded();
}

void WindowSelectorController::OnSelectionStarted() {
  if (!last_selection_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Ash.WindowSelector.TimeBetweenUse",
                             base::Time::Now() - last_selection_time_);
  }
}

}  // namespace ash

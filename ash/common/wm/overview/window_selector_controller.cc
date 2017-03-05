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
#include "base/metrics/histogram_macros.h"

namespace ash {

WindowSelectorController::WindowSelectorController() {}

WindowSelectorController::~WindowSelectorController() {
  // Destroy widgets that may be still animating if shell shuts down soon after
  // exiting overview mode.
  for (std::unique_ptr<DelayedAnimationObserver>& animation_observer :
       delayed_animations_) {
    animation_observer->Shutdown();
  }
}

// static
bool WindowSelectorController::CanSelect() {
  // Don't allow a window overview if the screen is locked or a modal dialog is
  // open or running in kiosk app session.
  WmShell* wm_shell = WmShell::Get();
  SessionStateDelegate* session_state_delegate =
      wm_shell->GetSessionStateDelegate();
  SystemTrayDelegate* system_tray_delegate = wm_shell->system_tray_delegate();
  return session_state_delegate->IsActiveUserSessionStarted() &&
         !session_state_delegate->IsScreenLocked() &&
         !wm_shell->IsSystemModalWindowOpen() && !wm_shell->IsPinned() &&
         system_tray_delegate->GetUserLoginStatus() != LoginStatus::KIOSK_APP &&
         system_tray_delegate->GetUserLoginStatus() !=
             LoginStatus::ARC_KIOSK_APP;
}

bool WindowSelectorController::ToggleOverview() {
  if (IsSelecting()) {
    OnSelectionEnded();
  } else {
    // Don't start overview if window selection is not allowed.
    if (!CanSelect())
      return false;

    std::vector<WmWindow*> windows =
        WmShell::Get()->mru_window_tracker()->BuildMruWindowList();
    auto end =
        std::remove_if(windows.begin(), windows.end(),
                       std::not1(std::ptr_fun(&WindowSelector::IsSelectable)));
    windows.resize(end - windows.begin());

    // Don't enter overview mode with no windows.
    if (windows.empty())
      return false;

    WmShell::Get()->OnOverviewModeStarting();
    window_selector_.reset(new WindowSelector(this));
    window_selector_->Init(windows);
    OnSelectionStarted();
  }
  return true;
}

bool WindowSelectorController::IsSelecting() const {
  return window_selector_.get() != NULL;
}

void WindowSelectorController::IncrementSelection(int increment) {
  DCHECK(IsSelecting());
  window_selector_->IncrementSelection(increment);
}

bool WindowSelectorController::AcceptSelection() {
  DCHECK(IsSelecting());
  return window_selector_->AcceptSelection();
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

void WindowSelectorController::AddDelayedAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  animation_observer->SetOwner(this);
  delayed_animations_.push_back(std::move(animation_observer));
}

void WindowSelectorController::RemoveAndDestroyAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  class IsEqual {
   public:
    explicit IsEqual(DelayedAnimationObserver* animation_observer)
        : animation_observer_(animation_observer) {}
    bool operator()(const std::unique_ptr<DelayedAnimationObserver>& other) {
      return (other.get() == animation_observer_);
    }

   private:
    const DelayedAnimationObserver* animation_observer_;
  };
  delayed_animations_.erase(
      std::remove_if(delayed_animations_.begin(), delayed_animations_.end(),
                     IsEqual(animation_observer)),
      delayed_animations_.end());
}

void WindowSelectorController::OnSelectionStarted() {
  if (!last_selection_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Ash.WindowSelector.TimeBetweenUse",
                             base::Time::Now() - last_selection_time_);
  }
}

}  // namespace ash

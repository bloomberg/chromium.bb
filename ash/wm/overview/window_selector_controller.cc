// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include <vector>

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm_window.h"
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
  SessionController* session_controller = Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsScreenLocked() &&
         !ShellPort::Get()->IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !session_controller->IsKioskSession();
}

bool WindowSelectorController::ToggleOverview() {
  if (IsSelecting()) {
    OnSelectionEnded();
  } else {
    // Don't start overview if window selection is not allowed.
    if (!CanSelect())
      return false;

    std::vector<WmWindow*> windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList();
    auto end =
        std::remove_if(windows.begin(), windows.end(),
                       std::not1(std::ptr_fun(&WindowSelector::IsSelectable)));
    windows.resize(end - windows.begin());

    // Don't enter overview mode with no windows.
    if (windows.empty())
      return false;

    Shell::Get()->NotifyOverviewModeStarting();
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
  Shell::Get()->NotifyOverviewModeEnded();
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

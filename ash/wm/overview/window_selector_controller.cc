// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
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

    auto windows = Shell::Get()->mru_window_tracker()->BuildMruWindowList();

    // System modal windows will be hidden in overview.
    std::vector<aura::Window*> hide_windows;
    for (auto* window : windows) {
      if (!window->GetProperty(ash::kShowInOverviewKey))
        hide_windows.push_back(window);
    }

    auto end = std::remove_if(
        windows.begin(), windows.end(), [&hide_windows](aura::Window* window) {
          return std::find(hide_windows.begin(), hide_windows.end(), window) !=
                 hide_windows.end();
        });
    windows.resize(end - windows.begin());

    // Other non-selectable windows will be ignored in overview.
    end =
        std::remove_if(windows.begin(), windows.end(),
                       std::not1(std::ptr_fun(&WindowSelector::IsSelectable)));
    windows.resize(end - windows.begin());

    if (!Shell::Get()->IsSplitViewModeActive()) {
      // Don't enter overview with no window if the split view mode is inactive.
      if (windows.empty())
        return false;
    } else {
      // Don't enter overview with less than 1 window if the split view mode is
      // active.
      if (windows.size() <= 1)
        return false;

      // Remove the default snapped window from the window list. The default
      // snapped window occupies one side of the screen, while the other windows
      // occupy the other side of the screen in overview mode. The default snap
      // position is the position where the window was first snapped. See
      // |default_snap_position_| in SplitViewController for more detail.
      aura::Window* default_snapped_window =
          Shell::Get()->split_view_controller()->GetDefaultSnappedWindow();
      auto iter =
          std::find(windows.begin(), windows.end(), default_snapped_window);
      DCHECK(iter != windows.end());
      windows.erase(iter);
    }

    Shell::Get()->NotifyOverviewModeStarting();
    window_selector_.reset(new WindowSelector(this));
    window_selector_->Init(windows, hide_windows);
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

std::vector<aura::Window*>
WindowSelectorController::GetWindowsListInOverviewGridsForTesting() {
  std::vector<aura::Window*> windows;
  for (const std::unique_ptr<WindowGrid>& grid :
       window_selector_->grid_list_for_testing()) {
    for (const auto& window_selector_item : grid->window_list())
      windows.push_back(window_selector_item->GetWindow());
  }
  return windows;
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

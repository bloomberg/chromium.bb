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
#include "ash/wm/root_window_finder.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
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

  if (window_selector_.get()) {
    window_selector_->Shutdown();
    window_selector_.reset();
  }
}

// static
bool WindowSelectorController::CanSelect() {
  // Don't allow a window overview if the user session is not active (e.g.
  // locked or in user-adding screen) or a modal dialog is open or running in
  // kiosk app session.
  SessionController* session_controller = Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !ShellPort::Get()->IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !session_controller->IsRunningInAppMode();
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

void WindowSelectorController::OnOverviewButtonTrayLongPressed(
    const gfx::Point& event_location) {
  // Depending on the state of the windows and split view, a long press has many
  // different results.
  // 1. Already in split view - exit split view.
  // 2. Not in overview mode - enter split view iff
  //     a) there is an active window
  //     b) there are at least two windows in the mru list
  //     c) the active window is snappable
  // 3. In overview mode - enter split view iff
  //     a) there are at least two windows in the mru list
  //     b) the first window in the mru list is snappable

  auto* split_view_controller = Shell::Get()->split_view_controller();
  // Exit split view mode if we are already in it.
  if (split_view_controller->IsSplitViewModeActive()) {
    split_view_controller->EndSplitView();
    return;
  }

  auto* active_window = wm::GetActiveWindow();
  // Do nothing if there are no active windows, less than two windows to work
  // with, or the active window is not snappable.
  // TODO(sammiequon): Bounce the window if it is not snappable.
  if (!active_window ||
      Shell::Get()->mru_window_tracker()->BuildMruWindowList().size() < 2u ||
      !split_view_controller->CanSnap(active_window)) {
    return;
  }

  // If we are not in overview mode snap the window left and enter overview
  // mode.
  if (!IsSelecting()) {
    split_view_controller->SnapWindow(active_window, SplitViewController::LEFT);
    ToggleOverview();
    return;
  }

  // Currently in overview mode, with no snapped windows. Retrieve the first
  // window selector item and attempt to snap that window.
  DCHECK(window_selector_);
  WindowSelectorItem* item_to_snap = nullptr;
  WindowGrid* current_grid = window_selector_->GetGridWithRootWindow(
      wm::GetRootWindowAt(event_location));
  if (current_grid) {
    const auto& windows = current_grid->window_list();
    if (windows.size() > 0)
      item_to_snap = windows[0].get();
  }

  // Do nothing if no item was retrieved, or if the retrieved item is
  // unsnappable.
  // TODO(sammiequon): Bounce the window if it is not snappable.
  if (!item_to_snap ||
      !split_view_controller->CanSnap(item_to_snap->GetWindow())) {
    return;
  }

  // Snap the window selector item and remove it from the grid.
  item_to_snap->EnsureVisible();
  item_to_snap->RestoreWindow();
  aura::Window* window = item_to_snap->GetWindow();
  window_selector_->RemoveWindowSelectorItem(item_to_snap);
  split_view_controller->SnapWindow(window, SplitViewController::LEFT);
  window_selector_->SetBoundsForWindowGridsInScreen(
      split_view_controller->GetSnappedWindowBoundsInScreen(
          window, SplitViewController::RIGHT));
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
  // Don't delete |window_selector_| yet since the stack is still using it.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  window_selector_.release());
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

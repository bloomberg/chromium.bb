// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Returns true if |window| should be hidden when entering overview.
bool ShouldHideWindowInOverview(const aura::Window* window) {
  return !window->GetProperty(ash::kShowInOverviewKey);
}

// Returns true if |window| should be excluded from overview.
bool ShouldExcludeWindowFromOverview(const aura::Window* window) {
  if (ShouldHideWindowInOverview(window))
    return true;

  // Other non-selectable windows will be ignored in overview.
  if (!WindowSelector::IsSelectable(window))
    return true;

  // Remove the default snapped window from the window list. The default
  // snapped window occupies one side of the screen, while the other windows
  // occupy the other side of the screen in overview mode. The default snap
  // position is the position where the window was first snapped. See
  // |default_snap_position_| in SplitViewController for more detail.
  if (Shell::Get()->IsSplitViewModeActive() &&
      window ==
          Shell::Get()->split_view_controller()->GetDefaultSnappedWindow()) {
    return true;
  }

  return false;
}

}  // namespace

WindowSelectorController::WindowSelectorController() = default;

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
         !Shell::IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !session_controller->IsRunningInAppMode();
}

bool WindowSelectorController::ToggleOverview() {
  auto windows = Shell::Get()->mru_window_tracker()->BuildMruWindowList();

  // Hidden windows will be removed by ShouldExcludeWindowFromOverview so we
  // must copy them out first.
  std::vector<aura::Window*> hide_windows(windows.size());
  auto end = std::copy_if(windows.begin(), windows.end(), hide_windows.begin(),
                          ShouldHideWindowInOverview);
  hide_windows.resize(end - hide_windows.begin());

  end = std::remove_if(windows.begin(), windows.end(),
                       ShouldExcludeWindowFromOverview);
  windows.resize(end - windows.begin());

  if (IsSelecting()) {
    // Do not allow ending overview if we're in single split mode.
    if (windows.empty() && Shell::Get()->IsSplitViewModeActive())
      return true;
    OnSelectionEnded();
  } else {
    // Don't start overview if window selection is not allowed.
    if (!CanSelect())
      return false;

    // Don't enter overview with no windows to select from.
    if (!IsNewOverviewUi() && windows.empty())
      return false;

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
  // Do nothing if split view is not enabled.
  if (!SplitViewController::ShouldAllowSplitView())
    return;

  // Depending on the state of the windows and split view, a long press has many
  // different results.
  // 1. Already in split view - exit split view. Activate the left window if it
  // is snapped left or both sides. Activate the right window if it is snapped
  // right.
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
    // In some cases the window returned by wm::GetActiveWindow will be an item
    // in overview mode (maybe the overview mode text selection widget). The
    // active window may also be a transient descendant of the left or right
    // snapped window, in which we want to activate the transient window's
    // ancestor (left or right snapped window). Manually set |active_window| as
    // either the left or right window.
    aura::Window* active_window = wm::GetActiveWindow();
    while (::wm::GetTransientParent(active_window))
      active_window = ::wm::GetTransientParent(active_window);
    if (active_window != split_view_controller->left_window() &&
        active_window != split_view_controller->right_window()) {
      active_window = split_view_controller->GetDefaultSnappedWindow();
    }
    DCHECK(active_window);
    split_view_controller->EndSplitView();
    if (IsSelecting())
      ToggleOverview();
    wm::ActivateWindow(active_window);
    base::RecordAction(
        base::UserMetricsAction("Tablet_LongPressOverviewButtonExitSplitView"));
    return;
  }

  WindowSelectorItem* item_to_snap = nullptr;
  if (!IsSelecting()) {
    aura::Window* active_window = wm::GetActiveWindow();
    // Do nothing if there are no active windows or less than two windows to
    // work with.
    if (!active_window ||
        Shell::Get()->mru_window_tracker()->BuildMruWindowList().size() < 2u) {
      return;
    }

    // Show a toast if the window cannot be snapped.
    if (!split_view_controller->CanSnap(active_window)) {
      split_view_controller->ShowAppCannotSnapToast();
      return;
    }

    // If we are not in overview mode, enter overview mode and then find the
    // window item to snap.
    ToggleOverview();
    DCHECK(window_selector_);
    WindowGrid* current_grid =
        window_selector_->GetGridWithRootWindow(active_window->GetRootWindow());
    if (current_grid) {
      item_to_snap =
          current_grid->GetWindowSelectorItemContaining(active_window);
    }
  } else {
    // Currently in overview mode, with no snapped windows. Retrieve the first
    // window selector item and attempt to snap that window.
    DCHECK(window_selector_);
    WindowGrid* current_grid = window_selector_->GetGridWithRootWindow(
        wm::GetRootWindowAt(event_location));
    if (current_grid) {
      const auto& windows = current_grid->window_list();
      if (windows.size() > 1)
        item_to_snap = windows[0].get();
    }
  }

  // Do nothing if no item was retrieved, or if the retrieved item is
  // unsnappable.
  // TODO(sammiequon): Bounce the window if it is not snappable.
  if (!item_to_snap ||
      !split_view_controller->CanSnap(item_to_snap->GetWindow())) {
    return;
  }

  // Snap the window selector item and remove it from the grid.
  // The transform will be reset later after the window is snapped.
  item_to_snap->RestoreWindow(/*reset_transform=*/false);
  aura::Window* window = item_to_snap->GetWindow();
  const gfx::Rect item_bounds = item_to_snap->target_bounds();
  window_selector_->RemoveWindowSelectorItem(item_to_snap);
  split_view_controller->SnapWindow(window, SplitViewController::LEFT,
                                    item_bounds);
  window_selector_->SetBoundsForWindowGridsInScreen(
      split_view_controller->GetSnappedWindowBoundsInScreen(
          window, SplitViewController::RIGHT));
  base::RecordAction(
      base::UserMetricsAction("Tablet_LongPressOverviewButtonEnterSplitView"));
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
  if (is_shutting_down_)
    return;
  is_shutting_down_ = true;
  Shell::Get()->NotifyOverviewModeEnding();
  window_selector_->Shutdown();
  // Don't delete |window_selector_| yet since the stack is still using it.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  window_selector_.release());
  last_selection_time_ = base::Time::Now();
  Shell::Get()->NotifyOverviewModeEnded();
  is_shutting_down_ = false;
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

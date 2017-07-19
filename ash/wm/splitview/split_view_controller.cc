// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include "ash/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Returns true if |window| can be activated and snapped.
bool CanSnap(aura::Window* window) {
  return wm::CanActivateWindow(window) ? wm::GetWindowState(window)->CanSnap()
                                       : false;
}

}  // namespace

SplitViewController::SplitViewController() {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
}

SplitViewController::~SplitViewController() {
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  EndSplitView();
}

// static
bool SplitViewController::ShouldAllowSplitView() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTabletSplitView)) {
    return false;
  }
  if (!Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled()) {
    return false;
  }
  return true;
}

bool SplitViewController::IsSplitViewModeActive() const {
  return state_ != NO_SNAP;
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position) {
  DCHECK(window && CanSnap(window));

  if (state_ == NO_SNAP)
    Shell::Get()->NotifySplitViewModeStarting();

  State previous_state = state_;
  if (snap_position == LEFT) {
    if (left_window_ != window) {
      StopObserving(left_window_);
      left_window_ = window;
    }
    right_window_ = (window == right_window_) ? nullptr : right_window_;

    switch (state_) {
      case NO_SNAP:
        default_snap_position_ = LEFT;
        state_ = LEFT_SNAPPED;
        break;
      case LEFT_SNAPPED:
        state_ = LEFT_SNAPPED;
        break;
      case RIGHT_SNAPPED:
      case BOTH_SNAPPED:
        state_ = BOTH_SNAPPED;
        break;
    }
  } else if (snap_position == RIGHT) {
    if (right_window_ != window) {
      StopObserving(right_window_);
      right_window_ = window;
    }
    left_window_ = (window == left_window_) ? nullptr : left_window_;

    switch (state_) {
      case NO_SNAP:
        default_snap_position_ = RIGHT;
        state_ = RIGHT_SNAPPED;
        break;
      case RIGHT_SNAPPED:
        state_ = RIGHT_SNAPPED;
        break;
      case LEFT_SNAPPED:
      case BOTH_SNAPPED:
        state_ = BOTH_SNAPPED;
        break;
    }
  }

  StartObserving(window);
  const wm::WMEvent event((snap_position == LEFT) ? wm::WM_EVENT_SNAP_LEFT
                                                  : wm::WM_EVENT_SNAP_RIGHT);
  wm::GetWindowState(window)->OnWMEvent(&event);
  wm::ActivateWindow(window);

  NotifySplitViewStateChanged(previous_state, state_);
}

aura::Window* SplitViewController::GetDefaultSnappedWindow() {
  if (default_snap_position_ == LEFT)
    return left_window_;
  if (default_snap_position_ == RIGHT)
    return right_window_;
  return nullptr;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInParent(
    aura::Window* window,
    State snap_state) {
  if (snap_state == LEFT_SNAPPED)
    return GetLeftWindowBoundsInParent(window);
  else if (snap_state == RIGHT_SNAPPED)
    return GetRightWindowBoundsInParent(window);

  NOTREACHED();
  return gfx::Rect();
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    aura::Window* window,
    State snap_state) {
  if (snap_state == LEFT_SNAPPED)
    return GetLeftWindowBoundsInScreen(window);
  else if (snap_state == RIGHT_SNAPPED)
    return GetRightWindowBoundsInScreen(window);

  NOTREACHED();
  return gfx::Rect();
}

gfx::Rect SplitViewController::GetDisplayWorkAreaBoundsInParent(
    aura::Window* window) {
  aura::Window* root_window = window->GetRootWindow();
  return ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_DefaultContainer));
}

gfx::Rect SplitViewController::GetDisplayWorkAreaBoundsInScreen(
    aura::Window* window) {
  gfx::Rect bounds = GetDisplayWorkAreaBoundsInParent(window);
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

void SplitViewController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SplitViewController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SplitViewController::OnWindowDestroying(aura::Window* window) {
  // If one of the snapped window gets closed, end the split view mode. The
  // behavior might change in the future.
  DCHECK(window == left_window_ || window == right_window_);
  EndSplitView();
}

void SplitViewController::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::wm::WindowStateType old_type) {
  if (window_state->IsFullscreen() || window_state->IsMinimized() ||
      window_state->IsMaximized()) {
    // TODO(xdai): Decide what to do if one of the snapped windows gets
    // minimized /maximized / full-screened. For now we end the split view mode
    // for simplicity.
    EndSplitView();
  }
}

void SplitViewController::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  if (!IsSplitViewModeActive())
    return;

  // If |gained_active| was activated as a side effect of a window disposition
  // change, do nothing. For example, when a snapped window is closed, another
  // window will be activated before OnWindowDestroying() is called. We should
  // not try to snap another window in this case.
  if (reason == ActivationReason::WINDOW_DISPOSITION_CHANGED)
    return;

  // Only snap window that can be snapped but hasn't been snapped.
  if (!gained_active || gained_active == left_window_ ||
      gained_active == right_window_ || !CanSnap(gained_active)) {
    return;
  }

  // Only window in MRU list can be snapped.
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();
  if (std::find(windows.begin(), windows.end(), gained_active) == windows.end())
    return;

  // Snap the window on the non-default side of the screen if split view mode
  // is active.
  if (default_snap_position_ == LEFT)
    SnapWindow(gained_active, SplitViewController::RIGHT);
  else
    SnapWindow(gained_active, SplitViewController::LEFT);
}

void SplitViewController::OnOverviewModeStarting() {
  // If split view mode is active, reset |state_| to make it be able to select
  // another window from overview window grid.
  if (IsSplitViewModeActive()) {
    State previous_state = state_;
    if (default_snap_position_ == LEFT) {
      StopObserving(right_window_);
      state_ = LEFT_SNAPPED;
    } else {
      StopObserving(left_window_);
      state_ = RIGHT_SNAPPED;
    }
    NotifySplitViewStateChanged(previous_state, state_);
  }
}

void SplitViewController::OnOverviewModeEnded() {
  // If split view mode is active but only has one snapped window, use the MRU
  // window list to auto select another window to snap.
  if (IsSplitViewModeActive() && state_ != BOTH_SNAPPED) {
    aura::Window::Windows windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList();
    for (auto* window : windows) {
      if (CanSnap(window) && window != GetDefaultSnappedWindow()) {
        if (default_snap_position_ == LEFT)
          SnapWindow(window, SplitViewController::RIGHT);
        else
          SnapWindow(window, SplitViewController::LEFT);
        break;
      }
    }
  }
}

void SplitViewController::EndSplitView() {
  StopObserving(left_window_);
  StopObserving(right_window_);
  left_window_ = nullptr;
  right_window_ = nullptr;
  default_snap_position_ = LEFT;
  divider_position_ = -1;

  State previous_state = state_;
  state_ = NO_SNAP;
  NotifySplitViewStateChanged(previous_state, state_);

  Shell::Get()->NotifySplitViewModeEnded();
}

void SplitViewController::StartObserving(aura::Window* window) {
  if (window && !window->HasObserver(this)) {
    window->AddObserver(this);
    wm::GetWindowState(window)->AddObserver(this);
  }
}

void SplitViewController::StopObserving(aura::Window* window) {
  if (window && window->HasObserver(this)) {
    window->RemoveObserver(this);
    wm::GetWindowState(window)->RemoveObserver(this);
  }
}

void SplitViewController::NotifySplitViewStateChanged(State previous_state,
                                                      State state) {
  if (previous_state == state)
    return;
  for (Observer& observer : observers_)
    observer.OnSplitViewStateChanged(previous_state, state);
}

gfx::Rect SplitViewController::GetLeftWindowBoundsInParent(
    aura::Window* window) {
  gfx::Rect bounds = GetLeftWindowBoundsInScreen(window);
  ::wm::ConvertRectFromScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetRightWindowBoundsInParent(
    aura::Window* window) {
  gfx::Rect bounds = GetRightWindowBoundsInScreen(window);
  ::wm::ConvertRectFromScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetLeftWindowBoundsInScreen(
    aura::Window* window) {
  const gfx::Rect display_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(window);
  if (divider_position_ < 0) {
    divider_position_ =
        display_bounds_in_screen.x() + display_bounds_in_screen.width() * 0.5f;
  }
  return gfx::Rect(display_bounds_in_screen.x(), display_bounds_in_screen.y(),
                   divider_position_ - display_bounds_in_screen.x(),
                   display_bounds_in_screen.height());
}

gfx::Rect SplitViewController::GetRightWindowBoundsInScreen(
    aura::Window* window) {
  const gfx::Rect display_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(window);
  if (divider_position_ < 0) {
    divider_position_ =
        display_bounds_in_screen.x() + display_bounds_in_screen.width() * 0.5f;
  }
  return gfx::Rect(divider_position_, display_bounds_in_screen.y(),
                   display_bounds_in_screen.x() +
                       display_bounds_in_screen.width() - divider_position_,
                   display_bounds_in_screen.height());
}

}  // namespace ash

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"

#include <memory>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"
#include "ash/wm/tablet_mode/tablet_mode_backdrop_delegate_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_event_handler.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Exits overview mode if it is currently active.
void CancelOverview() {
  OverviewController* controller = Shell::Get()->overview_controller();
  if (controller->IsSelecting())
    controller->OnSelectionEnded();
}

}  // namespace

TabletModeWindowManager::~TabletModeWindowManager() {
  // Overview mode needs to be ended before exiting tablet mode to prevent
  // transforming windows which are currently in
  // overview: http://crbug.com/366605
  CancelOverview();
  for (aura::Window* window : added_windows_)
    window->RemoveObserver(this);
  added_windows_.clear();
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::Get()->split_view_controller()->RemoveObserver(this);
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  ArrangeWindowsForDesktopMode();
}

int TabletModeWindowManager::GetNumberOfManagedWindows() {
  return window_state_map_.size();
}

void TabletModeWindowManager::AddWindow(aura::Window* window) {
  // Only add the window if it is a direct dependent of a container window
  // and not yet tracked.
  if (base::ContainsKey(window_state_map_, window) ||
      !IsContainerWindow(window->parent())) {
    return;
  }

  TrackWindow(window);
}

void TabletModeWindowManager::WindowStateDestroyed(aura::Window* window) {
  // We come here because the tablet window state object was destroyed. It was
  // destroyed either because ForgetWindow() was called, or because its
  // associated window was destroyed. In both cases, the window must has removed
  // TabletModeWindowManager as an observer.
  DCHECK(!window->HasObserver(this));

  // The window state object might have been removed in OnWindowDestroying().
  auto it = window_state_map_.find(window);
  if (it != window_state_map_.end())
    window_state_map_.erase(it);
}

void TabletModeWindowManager::OnSplitViewModeEnded() {
  switch (Shell::Get()->split_view_controller()->end_reason()) {
    case SplitViewController::EndReason::kNormal:
    case SplitViewController::EndReason::kUnsnappableWindowActivated:
      break;
    case SplitViewController::EndReason::kHomeLauncherPressed:
    case SplitViewController::EndReason::kActiveUserChanged:
      // For the case of kHomeLauncherPressed, the home launcher will minimize
      // the snapped windows after ending splitview, so avoid maximizing them
      // here. For the case of kActiveUserChanged, the snapped windows will be
      // used to restore the splitview layout when switching back, and it is
      // already too late to maximize them anyway (the for loop below would
      // iterate over windows in the newly activated user session).
      return;
  }

  // Maximize all snapped windows upon exiting split view mode. Note the snapped
  // window might not be tracked in our |window_state_map_|.
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();
  for (auto* window : windows) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    if (window_state->IsSnapped()) {
      wm::WMEvent event(wm::WM_EVENT_MAXIMIZE);
      window_state->OnWMEvent(&event);
    }
  }
}

void TabletModeWindowManager::OnOverviewModeStarting() {
  aura::Window* default_snapped_window =
      Shell::Get()->split_view_controller()->GetDefaultSnappedWindow();
  for (auto& pair : window_state_map_) {
    aura::Window* window = pair.first;
    if (window == default_snapped_window)
      continue;
    SetDeferBoundsUpdates(window, /*defer_bounds_updates=*/true);
  }
}

void TabletModeWindowManager::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  exit_overview_by_window_drag_ =
      overview_session->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kWindowDragged;
}

void TabletModeWindowManager::OnOverviewModeEnded() {
  for (auto& pair : window_state_map_) {
    // We don't want any animation if overview exits because of dragging a
    // window from top, including the window update bounds animation. Set the
    // animation tween type to ZERO for all the other windows except the dragged
    // window(active window). Then the dragged window can still be animated to
    // its target bounds but all the other windows' bounds will be updated at
    // the end of the animation.
    pair.second->set_use_zero_animation_type(
        exit_overview_by_window_drag_ &&
        !wm::GetWindowState(pair.first)->IsActive());

    SetDeferBoundsUpdates(pair.first, /*defer_bounds_updates=*/false);
    // SetDeferBoundsUpdates is called with /*defer_bounds_updates=*/false
    // hence the window bounds is updated with proper zero animation type
    // flag. Reset the flag here so that it does not affect window bounds
    // update later.
    pair.second->set_use_zero_animation_type(false);
  }
}

void TabletModeWindowManager::OnWindowDestroying(aura::Window* window) {
  if (IsContainerWindow(window)) {
    // container window can be removed on display destruction.
    window->RemoveObserver(this);
    observed_container_windows_.erase(window);
  } else if (base::ContainsKey(added_windows_, window)) {
    // Added window was destroyed before being shown.
    added_windows_.erase(window);
    window->RemoveObserver(this);
  } else {
    // If a known window gets destroyed we need to remove all knowledge about
    // it.
    ForgetWindow(window, true /* destroyed */);
  }
}

void TabletModeWindowManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // A window can get removed and then re-added by a drag and drop operation.
  if (params.new_parent && IsContainerWindow(params.new_parent) &&
      !base::ContainsKey(window_state_map_, params.target)) {
    // Don't register the window if the window is invisible. Instead,
    // wait until it becomes visible because the client may update the
    // flag to control if the window should be added.
    if (!params.target->IsVisible()) {
      if (!base::ContainsKey(added_windows_, params.target)) {
        added_windows_.insert(params.target);
        params.target->AddObserver(this);
      }
      return;
    }
    TrackWindow(params.target);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (base::ContainsKey(window_state_map_, params.target)) {
      wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
      wm::GetWindowState(params.target)->OnWMEvent(&event);
    }
  }
}

void TabletModeWindowManager::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  // Stop managing |window| if the always-on-top property is added.
  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    ForgetWindow(window, false /* destroyed */);
  }
}

void TabletModeWindowManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!IsContainerWindow(window))
    return;

  auto* session = Shell::Get()->overview_controller()->overview_session();
  if (session)
    session->SuspendReposition();

  // Reposition all non maximizeable windows.
  for (auto& pair : window_state_map_) {
    pair.second->UpdateWindowPosition(wm::GetWindowState(pair.first),
                                      /*animate=*/false);
  }
  if (session)
    session->ResumeReposition();
}

void TabletModeWindowManager::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  // Skip if it's already managed.
  if (base::ContainsKey(window_state_map_, window))
    return;

  if (IsContainerWindow(window->parent()) &&
      base::ContainsKey(added_windows_, window) && visible) {
    added_windows_.erase(window);
    window->RemoveObserver(this);
    TrackWindow(window);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (base::ContainsKey(window_state_map_, window)) {
      wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
      wm::GetWindowState(window)->OnWMEvent(&event);
    }
  }
}

void TabletModeWindowManager::OnDisplayAdded(const display::Display& display) {
  DisplayConfigurationChanged();
}

void TabletModeWindowManager::OnDisplayRemoved(
    const display::Display& display) {
  DisplayConfigurationChanged();
}

void TabletModeWindowManager::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  // It might be possible that split view mode and overview mode are active at
  // the same time. We need make sure the snapped windows bounds can be updated
  // immediately.
  // TODO(xdai): In overview mode, if we snap two windows to the same position
  // one by one, we need to restore the first window's |defer_bounds_updates_|
  // state here. It's unnecessary now since we don't insert the first window
  // back to the overview grid but we probably should do so in the future.
  if (state != SplitViewController::NO_SNAP) {
    SplitViewController* split_view_controller =
        Shell::Get()->split_view_controller();
    if (split_view_controller->left_window())
      SetDeferBoundsUpdates(split_view_controller->left_window(), false);
    if (split_view_controller->right_window())
      SetDeferBoundsUpdates(split_view_controller->right_window(), false);
  } else {
    // If split view mode is ended when overview mode is still active, defer
    // all bounds change until overview mode is ended.
    if (Shell::Get()->overview_controller()->IsSelecting()) {
      for (auto& pair : window_state_map_)
        SetDeferBoundsUpdates(pair.first, true);
    }
  }
}

void TabletModeWindowManager::SetIgnoreWmEventsForExit() {
  for (auto& pair : window_state_map_)
    pair.second->set_ignore_wm_events(true);
}

TabletModeWindowManager::TabletModeWindowManager() {
  // The overview mode needs to be ended before the tablet mode is started. To
  // guarantee the proper order, it will be turned off from here.
  CancelOverview();

  ArrangeWindowsForTabletMode();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->split_view_controller()->AddObserver(this);
  event_handler_ = std::make_unique<wm::TabletModeEventHandler>();
}

void TabletModeWindowManager::ArrangeWindowsForTabletMode() {
  // |split_view_eligible_windows| is for determining split view layout.
  // |activatable_windows| includes all windows to be tracked, and that includes
  // windows on the lock screen via |scoped_skip_user_session_blocked_check|.
  MruWindowTracker::WindowList split_view_eligible_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList();
  ScopedSkipUserSessionBlockedCheck scoped_skip_user_session_blocked_check;
  MruWindowTracker::WindowList activatable_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();

  // If split_view_eligible_windows[0] does not exist, cannot be snapped in
  // split view, or is ARC or not snapped, then just maximize all windows.
  if (split_view_eligible_windows.empty() ||
      !CanSnapInSplitview(split_view_eligible_windows[0]) ||
      static_cast<ash::AppType>(split_view_eligible_windows[0]->GetProperty(
          aura::client::kAppType)) == AppType::ARC_APP ||
      !wm::GetWindowState(split_view_eligible_windows[0])->IsSnapped()) {
    for (auto* window : activatable_windows)
      TrackWindow(window);
    return;
  }

  // Carry over split_view_eligible_windows[0] to split view, along with
  // split_view_eligible_windows[1] if it exists, is snapped on the opposite
  // side, can be snapped in split view, and is not ARC.
  const bool prev_win_eligible =
      split_view_eligible_windows.size() > 1u &&
      CanSnapInSplitview(split_view_eligible_windows[1]) &&
      static_cast<ash::AppType>(split_view_eligible_windows[1]->GetProperty(
          aura::client::kAppType)) != AppType::ARC_APP;
  std::vector<SplitViewController::SnapPosition> snap_positions;
  if (wm::GetWindowState(split_view_eligible_windows[0])->GetStateType() ==
      mojom::WindowStateType::LEFT_SNAPPED) {
    // split_view_eligible_windows[0] goes on the left.
    snap_positions.push_back(SplitViewController::LEFT);

    if (prev_win_eligible &&
        wm::GetWindowState(split_view_eligible_windows[1])->GetStateType() ==
            mojom::WindowStateType::RIGHT_SNAPPED) {
      // split_view_eligible_windows[1] goes on the right.
      snap_positions.push_back(SplitViewController::RIGHT);
    }
  } else {
    DCHECK_EQ(
        mojom::WindowStateType::RIGHT_SNAPPED,
        wm::GetWindowState(split_view_eligible_windows[0])->GetStateType());

    // split_view_eligible_windows[0] goes on the right.
    snap_positions.push_back(SplitViewController::RIGHT);

    if (prev_win_eligible &&
        wm::GetWindowState(split_view_eligible_windows[1])->GetStateType() ==
            mojom::WindowStateType::LEFT_SNAPPED) {
      // split_view_eligible_windows[1] goes on the left.
      snap_positions.push_back(SplitViewController::LEFT);
    }
  }

  for (auto* window : activatable_windows) {
    bool snap = false;
    for (size_t i = 0u; i < snap_positions.size(); ++i) {
      if (window == split_view_eligible_windows[i]) {
        snap = true;
        break;
      }
    }
    TrackWindow(window, snap, /*animate_bounds_on_attach=*/false);
  }
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  for (size_t i = 0u; i < snap_positions.size(); ++i) {
    split_view_controller->SnapWindow(split_view_eligible_windows[i],
                                      snap_positions[i]);
  }
}

void TabletModeWindowManager::ArrangeWindowsForDesktopMode() {
  while (window_state_map_.size())
    ForgetWindow(window_state_map_.begin()->first, false /* destroyed */);
}

void TabletModeWindowManager::SetDeferBoundsUpdates(aura::Window* window,
                                                    bool defer_bounds_updates) {
  auto iter = window_state_map_.find(window);
  if (iter != window_state_map_.end())
    iter->second->SetDeferBoundsUpdates(defer_bounds_updates);
}

void TabletModeWindowManager::TrackWindow(aura::Window* window,
                                          bool snap,
                                          bool animate_bounds_on_attach) {
  if (!ShouldHandleWindow(window))
    return;

  DCHECK(!base::ContainsKey(window_state_map_, window));
  window->AddObserver(this);

  // We create and remember a tablet mode state which will attach itself to
  // the provided state object.
  window_state_map_[window] =
      new TabletModeWindowState(window, this, snap, animate_bounds_on_attach);
}

void TabletModeWindowManager::ForgetWindow(aura::Window* window,
                                           bool destroyed) {
  added_windows_.erase(window);
  window->RemoveObserver(this);

  WindowToState::iterator it = window_state_map_.find(window);
  // A window may not be registered yet if the observer was
  // registered in OnWindowHierarchyChanged.
  if (it == window_state_map_.end())
    return;

  if (destroyed) {
    // If the window is to-be-destroyed, remove it from |window_state_map_|
    // immidietely. Otherwise it's possible to send a WMEvent to the to-be-
    // destroyed window.  Note we should not restore its old previous window
    // state object here since it will send unnecessary window state change
    // events. The tablet window state object and the old window state object
    // will be both deleted when the window is destroyed.
    window_state_map_.erase(it);
  } else {
    // By telling the state object to revert, it will switch back the old
    // State object and destroy itself, calling WindowStateDestroyed().
    it->second->LeaveTabletMode(wm::GetWindowState(it->first));
    DCHECK(!base::ContainsKey(window_state_map_, window));
  }
}

bool TabletModeWindowManager::ShouldHandleWindow(aura::Window* window) {
  DCHECK(window);

  // Windows with the always-on-top property should be free-floating and thus
  // not managed by us.
  if (window->GetProperty(aura::client::kAlwaysOnTopKey))
    return false;

  // If the changing bounds in the maximized/fullscreen is allowed, then
  // let the client manage it even in tablet mode.
  if (!wm::GetWindowState(window) ||
      wm::GetWindowState(window)->allow_set_bounds_direct()) {
    return false;
  }

  return window->type() == aura::client::WINDOW_TYPE_NORMAL;
}

void TabletModeWindowManager::AddWindowCreationObservers() {
  DCHECK(observed_container_windows_.empty());
  // Observe window activations/creations in the default containers on all root
  // windows.
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    aura::Window* default_container =
        root->GetChildById(kShellWindowId_DefaultContainer);
    DCHECK(!base::ContainsKey(observed_container_windows_, default_container));
    default_container->AddObserver(this);
    observed_container_windows_.insert(default_container);
  }
}

void TabletModeWindowManager::RemoveWindowCreationObservers() {
  for (aura::Window* window : observed_container_windows_)
    window->RemoveObserver(this);
  observed_container_windows_.clear();
}

void TabletModeWindowManager::DisplayConfigurationChanged() {
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
}

bool TabletModeWindowManager::IsContainerWindow(aura::Window* window) {
  return base::ContainsKey(observed_container_windows_, window);
}

void TabletModeWindowManager::EnableBackdropBehindTopWindowOnEachDisplay(
    bool enable) {
  // Inform the WorkspaceLayoutManager that we want to show a backdrop behind
  // the topmost window of its container.
  for (auto* controller : Shell::GetAllRootWindowControllers()) {
    controller->workspace_controller()->SetBackdropDelegate(
        enable ? std::make_unique<TabletModeBackdropDelegateImpl>() : nullptr);
  }
}

}  // namespace ash

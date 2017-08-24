// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"

#include "ash/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/tablet_mode/scoped_skip_user_session_blocked_check.h"
#include "ash/wm/tablet_mode/tablet_mode_backdrop_delegate_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_event_handler.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Exits overview mode if it is currently active.
void CancelOverview() {
  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
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
  display::Screen::GetScreen()->RemoveObserver(this);
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  RestoreAllWindows();
}

int TabletModeWindowManager::GetNumberOfManagedWindows() {
  return window_state_map_.size();
}

void TabletModeWindowManager::AddWindow(aura::Window* window) {
  // Only add the window if it is a direct dependent of a container window
  // and not yet tracked.
  if (!ShouldHandleWindow(window) ||
      base::ContainsKey(window_state_map_, window) ||
      !IsContainerWindow(window->parent())) {
    return;
  }

  MaximizeAndTrackWindow(window);
}

void TabletModeWindowManager::WindowStateDestroyed(aura::Window* window) {
  // At this time ForgetWindow() should already have been called. If not,
  // someone else must have replaced the "window manager's state object".
  DCHECK(!window->HasObserver(this));

  auto it = window_state_map_.find(window);
  DCHECK(it != window_state_map_.end());
  window_state_map_.erase(it);
}

void TabletModeWindowManager::OnOverviewModeStarting() {
  SetDeferBoundsUpdates(true);
}

void TabletModeWindowManager::OnOverviewModeEnded() {
  SetDeferBoundsUpdates(false);
}

void TabletModeWindowManager::OnSplitViewModeEnded() {
  // Maximize all snapped windows upon exiting split view mode.
  for (auto& pair : window_state_map_) {
    if (pair.second->GetType() == wm::WINDOW_STATE_TYPE_LEFT_SNAPPED ||
        pair.second->GetType() == wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED) {
      wm::WMEvent event(wm::WM_EVENT_MAXIMIZE);
      wm::GetWindowState(pair.first)->OnWMEvent(&event);
    }
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
    ForgetWindow(window);
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
    MaximizeAndTrackWindow(params.target);
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
    ForgetWindow(window);
  }
}

void TabletModeWindowManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (!IsContainerWindow(window))
    return;
  // Reposition all non maximizeable windows.
  for (auto& pair : window_state_map_)
    pair.second->UpdateWindowPosition(wm::GetWindowState(pair.first));
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
    MaximizeAndTrackWindow(window);
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

void TabletModeWindowManager::OnDisplayMetricsChanged(const display::Display&,
                                                      uint32_t) {
  // Nothing to do here.
}

void TabletModeWindowManager::SetIgnoreWmEventsForExit() {
  for (auto& pair : window_state_map_) {
    pair.second->set_ignore_wm_events(true);
  }
}

TabletModeWindowManager::TabletModeWindowManager() {
  // The overview mode needs to be ended before the tablet mode is started. To
  // guarantee the proper order, it will be turned off from here.
  CancelOverview();

  MaximizeAllWindows();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  event_handler_ = ShellPort::Get()->CreateTabletModeEventHandler();
}

void TabletModeWindowManager::MaximizeAllWindows() {
  // For maximizing and tracking windows, we want the build mru list to ignore
  // the fact that the windows are on the lock screen.
  ScopedSkipUserSessionBlockedCheck scoped_skip_user_session_blocked_check;
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();
  // Add all existing MRU windows.
  for (auto* window : windows)
    MaximizeAndTrackWindow(window);
}

void TabletModeWindowManager::RestoreAllWindows() {
  while (window_state_map_.size())
    ForgetWindow(window_state_map_.begin()->first);
}

void TabletModeWindowManager::SetDeferBoundsUpdates(bool defer_bounds_updates) {
  for (auto& pair : window_state_map_)
    pair.second->SetDeferBoundsUpdates(defer_bounds_updates);
}

void TabletModeWindowManager::MaximizeAndTrackWindow(aura::Window* window) {
  if (!ShouldHandleWindow(window))
    return;

  DCHECK(!base::ContainsKey(window_state_map_, window));
  window->AddObserver(this);

  // We create and remember a tablet mode state which will attach itself to
  // the provided state object.
  window_state_map_[window] = new TabletModeWindowState(window, this);
}

void TabletModeWindowManager::ForgetWindow(aura::Window* window) {
  added_windows_.erase(window);
  window->RemoveObserver(this);

  WindowToState::iterator it = window_state_map_.find(window);
  // A window may not be registered yet if the observer was
  // registered in OnWindowHierarchyChanged.
  if (it == window_state_map_.end()) {
    return;
  }

  // By telling the state object to revert, it will switch back the old
  // State object and destroy itself, calling WindowStateDestroyed().
  it->second->LeaveTabletMode(wm::GetWindowState(it->first));
  DCHECK(!base::ContainsKey(window_state_map_, window));
}

bool TabletModeWindowManager::ShouldHandleWindow(aura::Window* window) {
  DCHECK(window);

  // Windows with the always-on-top property should be free-floating and thus
  // not managed by us.
  if (window->GetProperty(aura::client::kAlwaysOnTopKey))
    return false;

  // If the changing bounds in the maximized/fullscreen is allowed, then
  // let the client manage it even in tablet mode.
  if (wm::GetWindowState(window)->allow_set_bounds_direct())
    return false;

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
        enable ? base::MakeUnique<TabletModeBackdropDelegateImpl>() : nullptr);
  }
}

}  // namespace ash

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/maximize_mode/maximize_mode_window_manager.h"

#include "ash/common/ash_switches.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/maximize_mode/maximize_mode_event_handler.h"
#include "ash/common/wm/maximize_mode/maximize_mode_window_state.h"
#include "ash/common/wm/maximize_mode/workspace_backdrop_delegate.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Exits overview mode if it is currently active.
void CancelOverview() {
  WindowSelectorController* controller =
      WmShell::Get()->window_selector_controller();
  if (controller->IsSelecting())
    controller->OnSelectionEnded();
}

}  // namespace

MaximizeModeWindowManager::~MaximizeModeWindowManager() {
  // Overview mode needs to be ended before exiting maximize mode to prevent
  // transforming windows which are currently in
  // overview: http://crbug.com/366605
  CancelOverview();

  WmShell::Get()->RemoveShellObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  RestoreAllWindows();
}

int MaximizeModeWindowManager::GetNumberOfManagedWindows() {
  return window_state_map_.size();
}

void MaximizeModeWindowManager::AddWindow(WmWindow* window) {
  // Only add the window if it is a direct dependent of a container window
  // and not yet tracked.
  if (!ShouldHandleWindow(window) || ContainsKey(window_state_map_, window) ||
      !IsContainerWindow(window->GetParent())) {
    return;
  }

  MaximizeAndTrackWindow(window);
}

void MaximizeModeWindowManager::WindowStateDestroyed(WmWindow* window) {
  // At this time ForgetWindow() should already have been called. If not,
  // someone else must have replaced the "window manager's state object".
  DCHECK(!window->HasObserver(this));

  auto it = window_state_map_.find(window);
  DCHECK(it != window_state_map_.end());
  window_state_map_.erase(it);
}

void MaximizeModeWindowManager::OnOverviewModeStarting() {
  if (backdrops_hidden_)
    return;

  EnableBackdropBehindTopWindowOnEachDisplay(false);
  SetDeferBoundsUpdates(true);
  backdrops_hidden_ = true;
}

void MaximizeModeWindowManager::OnOverviewModeEnded() {
  if (!backdrops_hidden_)
    return;

  backdrops_hidden_ = false;
  EnableBackdropBehindTopWindowOnEachDisplay(true);
  SetDeferBoundsUpdates(false);
}

void MaximizeModeWindowManager::OnWindowDestroying(WmWindow* window) {
  if (IsContainerWindow(window)) {
    // container window can be removed on display destruction.
    window->RemoveObserver(this);
    observed_container_windows_.erase(window);
  } else {
    // If a known window gets destroyed we need to remove all knowledge about
    // it.
    ForgetWindow(window);
  }
}

void MaximizeModeWindowManager::OnWindowTreeChanged(
    WmWindow* window,
    const TreeChangeParams& params) {
  // A window can get removed and then re-added by a drag and drop operation.
  if (params.new_parent && IsContainerWindow(params.new_parent) &&
      !ContainsKey(window_state_map_, params.target)) {
    MaximizeAndTrackWindow(params.target);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (ContainsKey(window_state_map_, params.target)) {
      wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
      params.target->GetWindowState()->OnWMEvent(&event);
    }
  }
}

void MaximizeModeWindowManager::OnWindowPropertyChanged(
    WmWindow* window,
    WmWindowProperty property) {
  // Stop managing |window| if the always-on-top property is added.
  if (property == WmWindowProperty::ALWAYS_ON_TOP && window->IsAlwaysOnTop())
    ForgetWindow(window);
}

void MaximizeModeWindowManager::OnWindowBoundsChanged(
    WmWindow* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (!IsContainerWindow(window))
    return;
  // Reposition all non maximizeable windows.
  for (auto& pair : window_state_map_)
    pair.second->UpdateWindowPosition(pair.first->GetWindowState());
}

void MaximizeModeWindowManager::OnDisplayAdded(
    const display::Display& display) {
  DisplayConfigurationChanged();
}

void MaximizeModeWindowManager::OnDisplayRemoved(
    const display::Display& display) {
  DisplayConfigurationChanged();
}

void MaximizeModeWindowManager::OnDisplayMetricsChanged(const display::Display&,
                                                        uint32_t) {
  // Nothing to do here.
}

MaximizeModeWindowManager::MaximizeModeWindowManager()
    : backdrops_hidden_(false) {
  // The overview mode needs to be ended before the maximize mode is started. To
  // guarantee the proper order, it will be turned off from here.
  CancelOverview();

  MaximizeAllWindows();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
  display::Screen::GetScreen()->AddObserver(this);
  WmShell::Get()->AddShellObserver(this);
  event_handler_ = WmShell::Get()->CreateMaximizeModeEventHandler();
}

void MaximizeModeWindowManager::MaximizeAllWindows() {
  MruWindowTracker::WindowList windows =
      WmShell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();
  // Add all existing Mru windows.
  for (WmWindow* window : windows)
    MaximizeAndTrackWindow(window);
}

void MaximizeModeWindowManager::RestoreAllWindows() {
  while (window_state_map_.size())
    ForgetWindow(window_state_map_.begin()->first);
}

void MaximizeModeWindowManager::SetDeferBoundsUpdates(
    bool defer_bounds_updates) {
  for (auto& pair : window_state_map_)
    pair.second->SetDeferBoundsUpdates(defer_bounds_updates);
}

void MaximizeModeWindowManager::MaximizeAndTrackWindow(WmWindow* window) {
  if (!ShouldHandleWindow(window))
    return;

  DCHECK(!ContainsKey(window_state_map_, window));
  window->AddObserver(this);

  // We create and remember a maximize mode state which will attach itself to
  // the provided state object.
  window_state_map_[window] = new MaximizeModeWindowState(window, this);
}

void MaximizeModeWindowManager::ForgetWindow(WmWindow* window) {
  WindowToState::iterator it = window_state_map_.find(window);

  // The following DCHECK could fail if our window state object was destroyed
  // earlier by someone else. However - at this point there is no other client
  // which replaces the state object and therefore this should not happen.
  DCHECK(it != window_state_map_.end());
  window->RemoveObserver(this);

  // By telling the state object to revert, it will switch back the old
  // State object and destroy itself, calling WindowStateDestroyed().
  it->second->LeaveMaximizeMode(it->first->GetWindowState());
  DCHECK(!ContainsKey(window_state_map_, window));
}

bool MaximizeModeWindowManager::ShouldHandleWindow(WmWindow* window) {
  DCHECK(window);

  // Windows with the always-on-top property should be free-floating and thus
  // not managed by us.
  if (window->IsAlwaysOnTop())
    return false;

  // Windows in the dock should not be managed by us.
  if (window->GetWindowState()->IsDocked())
    return false;

  return window->GetType() == ui::wm::WINDOW_TYPE_NORMAL;
}

void MaximizeModeWindowManager::AddWindowCreationObservers() {
  DCHECK(observed_container_windows_.empty());
  // Observe window activations/creations in the default containers on all root
  // windows.
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows()) {
    WmWindow* default_container =
        root->GetChildByShellWindowId(kShellWindowId_DefaultContainer);
    DCHECK(!ContainsKey(observed_container_windows_, default_container));
    default_container->AddObserver(this);
    observed_container_windows_.insert(default_container);
  }
}

void MaximizeModeWindowManager::RemoveWindowCreationObservers() {
  for (WmWindow* window : observed_container_windows_)
    window->RemoveObserver(this);
  observed_container_windows_.clear();
}

void MaximizeModeWindowManager::DisplayConfigurationChanged() {
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
}

bool MaximizeModeWindowManager::IsContainerWindow(WmWindow* window) {
  return ContainsKey(observed_container_windows_, window);
}

void MaximizeModeWindowManager::EnableBackdropBehindTopWindowOnEachDisplay(
    bool enable) {
  // This function should be a no-op if backdrops have been disabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableMaximizeModeWindowBackdrop)) {
    return;
  }

  if (backdrops_hidden_)
    return;

  // Inform the WorkspaceLayoutManager that we want to show a backdrop behind
  // the topmost window of its container.
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows()) {
    WmRootWindowController* controller = root->GetRootWindowController();
    WmWindow* default_container =
        root->GetChildByShellWindowId(kShellWindowId_DefaultContainer);
    controller->SetMaximizeBackdropDelegate(base::WrapUnique(
        enable ? new WorkspaceBackdropDelegate(default_container) : nullptr));
  }
}

}  // namespace ash

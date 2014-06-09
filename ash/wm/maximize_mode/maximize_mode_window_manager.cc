// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/maximize_mode/maximize_mode_window_state.h"
#include "ash/wm/maximize_mode/workspace_backdrop_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

// The height of the area in which a touch operation leads to exiting the
// full screen mode.
const int kLeaveFullScreenAreaHeightInPixel = 2;

// Exits overview mode if it is currently active.
void CancelOverview() {
  WindowSelectorController* controller =
      Shell::GetInstance()->window_selector_controller();
  if (controller && controller->IsSelecting())
    controller->OnSelectionEnded();
}

}  // namespace

MaximizeModeWindowManager::~MaximizeModeWindowManager() {
  // Overview mode needs to be ended before exiting maximize mode to prevent
  // transforming windows which are currently in
  // overview: http://crbug.com/366605
  CancelOverview();

  Shell::GetInstance()->RemovePreTargetHandler(this);
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetScreen()->RemoveObserver(this);
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  RestoreAllWindows();
}

int MaximizeModeWindowManager::GetNumberOfManagedWindows() {
  return window_state_map_.size();
}

void MaximizeModeWindowManager::WindowStateDestroyed(aura::Window* window) {
  // At this time ForgetWindow() should already have been called. If not,
  // someone else must have replaced the "window manager's state object".
  DCHECK(!window->HasObserver(this));

  WindowToState::iterator it = window_state_map_.find(window);
  DCHECK(it != window_state_map_.end());
  window_state_map_.erase(it);
}

void MaximizeModeWindowManager::OnOverviewModeStarting() {
  if (backdrops_hidden_)
    return;

  EnableBackdropBehindTopWindowOnEachDisplay(false);
  backdrops_hidden_ = true;
}

void MaximizeModeWindowManager::OnOverviewModeEnding() {
  if (!backdrops_hidden_)
    return;

  backdrops_hidden_ = false;
  EnableBackdropBehindTopWindowOnEachDisplay(true);
}

void MaximizeModeWindowManager::OnWindowDestroying(aura::Window* window) {
  // If a known window gets destroyed we need to remove all knowledge about it.
  if (!IsContainerWindow(window))
    ForgetWindow(window);
}

void MaximizeModeWindowManager::OnWindowAdded(aura::Window* window) {
  // A window can get removed and then re-added by a drag and drop operation.
  if (IsContainerWindow(window->parent()) &&
      window_state_map_.find(window) == window_state_map_.end()) {
    MaximizeAndTrackWindow(window);
    // When the state got added, the "WM_EVENT_ADDED_TO_WORKSPACE" event got
    // already sent and we have to notify our state again.
    if (window_state_map_.find(window) != window_state_map_.end()) {
      wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
      wm::GetWindowState(window)->OnWMEvent(&event);
    }
  }
}

void MaximizeModeWindowManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (!IsContainerWindow(window))
    return;
  // Reposition all non maximizeable windows.
  for (WindowToState::iterator it = window_state_map_.begin();
       it != window_state_map_.end();
       ++it) {
    it->second->UpdateWindowPosition(wm::GetWindowState(it->first), false);
  }
}

void MaximizeModeWindowManager::OnDisplayAdded(const gfx::Display& display) {
  DisplayConfigurationChanged();
}

void MaximizeModeWindowManager::OnDisplayRemoved(const gfx::Display& display) {
  DisplayConfigurationChanged();
}

void MaximizeModeWindowManager::OnDisplayMetricsChanged(const gfx::Display&,
                                                        uint32_t) {
  // Nothing to do here.
}

void MaximizeModeWindowManager::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;

  // Find the active window (from the primary screen) to un-fullscreen.
  aura::Window* window = wm::GetActiveWindow();
  if (!window)
    return;

  wm::WindowState* window_state = wm::GetWindowState(window);
  if (!window_state->IsFullscreen() || window_state->in_immersive_fullscreen())
    return;

  // Test that the touch happened in the top or bottom lines.
  int y = event->y();
  if (y >= kLeaveFullScreenAreaHeightInPixel &&
      y < (window->bounds().height() - kLeaveFullScreenAreaHeightInPixel)) {
    return;
  }

  // Leave full screen mode.
  event->StopPropagation();
  wm::WMEvent toggle_fullscreen(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen);
}

MaximizeModeWindowManager::MaximizeModeWindowManager()
      : backdrops_hidden_(false) {
  // The overview mode needs to be ended before the maximize mode is started. To
  // guarantee the proper order, it will be turned off from here.
  CancelOverview();

  MaximizeAllWindows();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
  Shell::GetScreen()->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
  Shell::GetInstance()->AddPreTargetHandler(this);
}

void MaximizeModeWindowManager::MaximizeAllWindows() {
  MruWindowTracker::WindowList windows =
      MruWindowTracker::BuildWindowList(false);
  // Add all existing Mru windows.
  for (MruWindowTracker::WindowList::iterator window = windows.begin();
      window != windows.end(); ++window) {
    MaximizeAndTrackWindow(*window);
  }
}

void MaximizeModeWindowManager::RestoreAllWindows() {
  while (window_state_map_.size())
    ForgetWindow(window_state_map_.begin()->first);
}

void MaximizeModeWindowManager::MaximizeAndTrackWindow(
    aura::Window* window) {
  if (!ShouldHandleWindow(window))
    return;

  DCHECK(window_state_map_.find(window) == window_state_map_.end());
  window->AddObserver(this);

  // We create and remember a maximize mode state which will attach itself to
  // the provided state object.
  window_state_map_[window] = new MaximizeModeWindowState(window, this);
}

void MaximizeModeWindowManager::ForgetWindow(aura::Window* window) {
  WindowToState::iterator it = window_state_map_.find(window);

  // The following DCHECK could fail if our window state object was destroyed
  // earlier by someone else. However - at this point there is no other client
  // which replaces the state object and therefore this should not happen.
  DCHECK(it != window_state_map_.end());
  window->RemoveObserver(this);

  // By telling the state object to revert, it will switch back the old
  // State object and destroy itself, calling WindowStateDerstroyed().
  it->second->LeaveMaximizeMode(wm::GetWindowState(it->first));
  DCHECK(window_state_map_.find(window) == window_state_map_.end());
}

bool MaximizeModeWindowManager::ShouldHandleWindow(aura::Window* window) {
  DCHECK(window);
  return window->type() == ui::wm::WINDOW_TYPE_NORMAL;
}

void MaximizeModeWindowManager::AddWindowCreationObservers() {
  DCHECK(observed_container_windows_.empty());
  // Observe window activations/creations in the default containers on all root
  // windows.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::Window* container =
        Shell::GetContainer(*iter, kShellWindowId_DefaultContainer);
    DCHECK(observed_container_windows_.find(container) ==
              observed_container_windows_.end());
    container->AddObserver(this);
    observed_container_windows_.insert(container);
  }
}

void MaximizeModeWindowManager::RemoveWindowCreationObservers() {
  for (std::set<aura::Window*>::iterator iter =
           observed_container_windows_.begin();
       iter != observed_container_windows_.end(); ++iter) {
    (*iter)->RemoveObserver(this);
  }
  observed_container_windows_.clear();
}

void MaximizeModeWindowManager::DisplayConfigurationChanged() {
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
}

bool MaximizeModeWindowManager::IsContainerWindow(aura::Window* window) {
  return observed_container_windows_.find(window) !=
             observed_container_windows_.end();
}

void MaximizeModeWindowManager::EnableBackdropBehindTopWindowOnEachDisplay(
    bool enable) {
  if (backdrops_hidden_)
    return;
  // Inform the WorkspaceLayoutManager that we want to show a backdrop behind
  // the topmost window of its container.
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (Shell::RootWindowControllerList::iterator iter = controllers.begin();
       iter != controllers.end(); ++iter) {
    RootWindowController* controller = *iter;
    aura::Window* container = Shell::GetContainer(
        controller->GetRootWindow(), kShellWindowId_DefaultContainer);
    controller->workspace_controller()->SetMaximizeBackdropDelegate(
        scoped_ptr<WorkspaceLayoutManagerDelegate>(
            enable ? new WorkspaceBackdropDelegate(container) : NULL));
  }
}

}  // namespace ash

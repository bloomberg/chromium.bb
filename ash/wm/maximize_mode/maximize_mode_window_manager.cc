// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/maximize_mode/workspace_backdrop_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

MaximizeModeWindowManager::~MaximizeModeWindowManager() {
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetScreen()->RemoveObserver(this);
  EnableBackdropBehindTopWindowOnEachDisplay(false);
  RemoveWindowCreationObservers();
  RestoreAllWindows();
  Shell::GetInstance()->OnMaximizeModeEnded();
}

int MaximizeModeWindowManager::GetNumberOfManagedWindows() {
  return initial_state_type_.size();
}

void MaximizeModeWindowManager::OnOverviewModeStarted() {
  if (backdrops_hidden_)
    return;

  EnableBackdropBehindTopWindowOnEachDisplay(false);
  backdrops_hidden_ = true;
}

void MaximizeModeWindowManager::OnOverviewModeEnded() {
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

void MaximizeModeWindowManager::OnWindowAdded(
    aura::Window* window) {
  // A window can get removed and then re-added by a drag and drop operation.
  if (IsContainerWindow(window->parent()) &&
      initial_state_type_.find(window) == initial_state_type_.end())
    MaximizeAndTrackWindow(window);
}

void MaximizeModeWindowManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (!IsContainerWindow(window))
    return;
  // Reposition all non maximizeable windows.
  for (WindowToStateType::iterator it = initial_state_type_.begin();
       it != initial_state_type_.end();
       ++it) {
    if (!CanMaximize(it->first))
      CenterWindow(it->first);
  }
}

void MaximizeModeWindowManager::OnDisplayBoundsChanged(
    const gfx::Display& display) {
  // Nothing to do here.
}

void MaximizeModeWindowManager::OnDisplayAdded(const gfx::Display& display) {
  DisplayConfigurationChanged();
}

void MaximizeModeWindowManager::OnDisplayRemoved(const gfx::Display& display) {
  DisplayConfigurationChanged();
}

MaximizeModeWindowManager::MaximizeModeWindowManager()
      : backdrops_hidden_(false) {
  // TODO(skuhne): Turn off the overview mode and full screen modes before
  // entering the MaximzieMode.
  MaximizeAllWindows();
  AddWindowCreationObservers();
  EnableBackdropBehindTopWindowOnEachDisplay(true);
  Shell::GetInstance()->OnMaximizeModeStarted();
  Shell::GetScreen()->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
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
  while (initial_state_type_.size())
    RestoreAndForgetWindow(initial_state_type_.begin()->first);
}

void MaximizeModeWindowManager::MaximizeAndTrackWindow(
    aura::Window* window) {
  if (!ShouldHandleWindow(window))
    return;

  DCHECK(initial_state_type_.find(window) == initial_state_type_.end());
  window->AddObserver(this);
  // Remember the state at the creation.
  wm::WindowState* window_state = wm::GetWindowState(window);
  wm::WindowStateType state = window_state->GetStateType();
  initial_state_type_[window] = state;
  // If it is not maximized yet we may need to maximize it now.
  if (state != wm::WINDOW_STATE_TYPE_MAXIMIZED) {
    if (!CanMaximize(window)) {
      // This window type should not be able to have a restore state set (since
      // it cannot maximize).
      DCHECK(!window_state->HasRestoreBounds());
      // Store the coordinates as restore coordinates.
      gfx::Rect initial_rect = window->bounds();
      if (window->parent())
        window_state->SetRestoreBoundsInParent(initial_rect);
      else
        window_state->SetRestoreBoundsInScreen(initial_rect);
      CenterWindow(window);
    } else {
      // Minimized windows can remain as they are.
      if (state != wm::WINDOW_STATE_TYPE_MINIMIZED)
        wm::GetWindowState(window)->Maximize();
    }
  }
  window_state->set_can_be_dragged(false);
}

void MaximizeModeWindowManager::RestoreAndForgetWindow(
    aura::Window* window) {
  wm::WindowStateType state = ForgetWindow(window);
  wm::WindowState* window_state = wm::GetWindowState(window);
  window_state->set_can_be_dragged(true);
  // Restore window if it can be restored.
  if (state != wm::WINDOW_STATE_TYPE_MAXIMIZED) {
    if (!CanMaximize(window)) {
      if (window_state->HasRestoreBounds()) {
        // TODO(skuhne): If the system shuts down in maximized mode, the proper
        // restore coordinates should get saved.
        gfx::Rect initial_bounds =
            window->parent() ? window_state->GetRestoreBoundsInParent() :
                               window_state->GetRestoreBoundsInScreen();
        window_state->ClearRestoreBounds();
        // TODO(skuhne): The screen might have changed and we should make sure
        // that the bounds are in a visible area.
        window->SetBounds(initial_bounds);
      }
    } else {
      // If the window neither was minimized or maximized and it is currently
      // not minimized, we restore it.
      if (state != wm::WINDOW_STATE_TYPE_MINIMIZED &&
          !wm::GetWindowState(window)->IsMinimized()) {
        wm::GetWindowState(window)->Restore();
      }
    }
  }
}

wm::WindowStateType MaximizeModeWindowManager::ForgetWindow(
    aura::Window* window) {
  WindowToStateType::iterator it = initial_state_type_.find(window);
  DCHECK(it != initial_state_type_.end());
  window->RemoveObserver(this);
  wm::WindowStateType state = it->second;
  initial_state_type_.erase(it);
  return state;
}

bool MaximizeModeWindowManager::ShouldHandleWindow(aura::Window* window) {
  DCHECK(window);
  return window->type() == ui::wm::WINDOW_TYPE_NORMAL;
}

bool MaximizeModeWindowManager::CanMaximize(aura::Window* window) {
  DCHECK(window);
  return wm::GetWindowState(window)->CanMaximize();
}

void MaximizeModeWindowManager::CenterWindow(aura::Window* window) {
  gfx::Size window_size = window->bounds().size();
  gfx::Rect work_area = gfx::Screen::GetScreenFor(window)->
      GetDisplayNearestWindow(window).work_area();

  gfx::Rect window_bounds(
      work_area.x() + (work_area.width() - window_size.width()) / 2,
      work_area.y() + (work_area.height() - window_size.height()) / 2,
      window_size.width(),
      window_size.height());

  window->SetBounds(window_bounds);
}

void MaximizeModeWindowManager::AddWindowCreationObservers() {
  DCHECK(observed_container_windows_.empty());
  // Observe window activations/creations in the default containers on all root
  // windows.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::Window* container = Shell::GetContainer(*iter,
        internal::kShellWindowId_DefaultContainer);
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
        controller->root_window(),
        internal::kShellWindowId_DefaultContainer);
    controller->workspace_controller()->SetMaximizeBackdropDelegate(
        scoped_ptr<WorkspaceLayoutManagerDelegate>(
            enable ? new WorkspaceBackdropDelegate(container) : NULL));
  }
}

}  // namespace internal
}  // namespace ash

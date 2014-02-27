// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"

#include "ash/shell.h"
#include "ash/switchable_windows.h"
#include "ash/wm/mru_window_tracker.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

MaximizeModeWindowManager::~MaximizeModeWindowManager() {
  Shell::GetScreen()->RemoveObserver(this);
  RemoveWindowCreationObservers();
  RestoreAllWindows();
}

int MaximizeModeWindowManager::GetNumberOfManagedWindows() {
  return initial_state_type_.size();
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

MaximizeModeWindowManager::MaximizeModeWindowManager() {
  MaximizeAllWindows();
  AddWindowCreationObservers();
  Shell::GetScreen()->AddObserver(this);
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
  WindowToStateType::iterator it = initial_state_type_.begin();
  while (it != initial_state_type_.end()) {
    RestoreAndForgetWindow(it->first);
    // |RestoreAndForgetWindow| will change the |initial_state_type_| list.
    it = initial_state_type_.begin();
  }
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
      // TODO(skuhne): Add a background cover layer.
    } else {
      // Minimized windows can remain as they are.
      if (state != wm::WINDOW_STATE_TYPE_MINIMIZED)
        wm::GetWindowState(window)->Maximize();
    }
  }
}

void MaximizeModeWindowManager::RestoreAndForgetWindow(
    aura::Window* window) {
  wm::WindowStateType state = ForgetWindow(window);
  // Restore window if it can be restored.
  if (state != wm::WINDOW_STATE_TYPE_MAXIMIZED) {
    if (!CanMaximize(window)) {
      // TODO(skuhne): Remove the background cover layer.
      wm::WindowState* window_state = wm::GetWindowState(window);
      if (window_state->HasRestoreBounds()) {
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
  // Observe window activations and switchable containers on all root windows
  // for newly created windows during overview.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
      aura::Window* container = Shell::GetContainer(*iter,
          kSwitchableWindowContainerIds[i]);
      DCHECK(observed_container_windows_.find(container) ==
                observed_container_windows_.end());
      container->AddObserver(this);
      observed_container_windows_.insert(container);
    }
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
  RemoveWindowCreationObservers();
  AddWindowCreationObservers();
}

bool MaximizeModeWindowManager::IsContainerWindow(aura::Window* window) {
  return observed_container_windows_.find(window) !=
             observed_container_windows_.end();
}

}  // namespace internal
}  // namespace ash

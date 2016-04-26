// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_layout_manager.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/lock_window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

LockLayoutManager::LockLayoutManager(aura::Window* window)
    : SnapToPixelLayoutManager(window),
      window_(window),
      root_window_(window->GetRootWindow()),
      is_observing_keyboard_(false) {
  Shell::GetInstance()->delegate()->AddVirtualKeyboardStateObserver(this);
  root_window_->AddObserver(this);
  if (keyboard::KeyboardController::GetInstance()) {
    keyboard::KeyboardController::GetInstance()->AddObserver(this);
    is_observing_keyboard_ = true;
  }
}

LockLayoutManager::~LockLayoutManager() {
  if (root_window_)
    root_window_->RemoveObserver(this);

  for (aura::Window::Windows::const_iterator it = window_->children().begin();
       it != window_->children().end(); ++it) {
    (*it)->RemoveObserver(this);
  }

  Shell::GetInstance()->delegate()->RemoveVirtualKeyboardStateObserver(this);

  if (keyboard::KeyboardController::GetInstance() && is_observing_keyboard_) {
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
    is_observing_keyboard_ = false;
  }
}

void LockLayoutManager::OnWindowResized() {
  const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
  AdjustWindowsForWorkAreaChange(&event);
}

void LockLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  child->AddObserver(this);

  // LockWindowState replaces default WindowState of a child.
  wm::WindowState* window_state = LockWindowState::SetLockWindowState(child);
  wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
}

void LockLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  child->RemoveObserver(this);
}

void LockLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}

void LockLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
}

void LockLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, requested_bounds);
  window_state->OnWMEvent(&event);
}

void LockLayoutManager::OnWindowHierarchyChanged(
    const WindowObserver::HierarchyChangeParams& params) {
}

void LockLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
}

void LockLayoutManager::OnWindowStackingChanged(aura::Window* window) {
}

void LockLayoutManager::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  if (root_window_ == window)
    root_window_ = NULL;
}

void LockLayoutManager::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  if (root_window_ == window) {
    const wm::WMEvent wm_event(wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED);
    AdjustWindowsForWorkAreaChange(&wm_event);
  }
}

void LockLayoutManager::OnVirtualKeyboardStateChanged(bool activated) {
  if (keyboard::KeyboardController::GetInstance()) {
    if (activated) {
      if (!is_observing_keyboard_) {
        keyboard::KeyboardController::GetInstance()->AddObserver(this);
        is_observing_keyboard_ = true;
      }
    } else {
      keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
      is_observing_keyboard_ = false;
    }
  }
}

void LockLayoutManager::OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) {
  keyboard_bounds_ = new_bounds;
  OnWindowResized();
}

void LockLayoutManager::AdjustWindowsForWorkAreaChange(
    const wm::WMEvent* event) {
  DCHECK(event->type() == wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED ||
         event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);

  for (aura::Window::Windows::const_iterator it = window_->children().begin();
       it != window_->children().end();
       ++it) {
    wm::GetWindowState(*it)->OnWMEvent(event);
  }
}

}  // namespace ash

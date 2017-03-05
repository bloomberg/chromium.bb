// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/lock_layout_manager.h"

#include "ash/common/wm/lock_window_state.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

LockLayoutManager::LockLayoutManager(WmWindow* window)
    : wm::WmSnapToPixelLayoutManager(),
      window_(window),
      root_window_(window->GetRootWindow()),
      is_observing_keyboard_(false) {
  WmShell::Get()->AddShellObserver(this);
  root_window_->aura_window()->AddObserver(this);
  if (keyboard::KeyboardController::GetInstance()) {
    keyboard::KeyboardController::GetInstance()->AddObserver(this);
    is_observing_keyboard_ = true;
  }
}

LockLayoutManager::~LockLayoutManager() {
  if (root_window_)
    root_window_->aura_window()->RemoveObserver(this);

  for (WmWindow* child : window_->GetChildren())
    child->aura_window()->RemoveObserver(this);

  WmShell::Get()->RemoveShellObserver(this);

  if (keyboard::KeyboardController::GetInstance() && is_observing_keyboard_) {
    keyboard::KeyboardController::GetInstance()->RemoveObserver(this);
    is_observing_keyboard_ = false;
  }
}

void LockLayoutManager::OnWindowResized() {
  const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
  AdjustWindowsForWorkAreaChange(&event);
}

void LockLayoutManager::OnWindowAddedToLayout(WmWindow* child) {
  child->aura_window()->AddObserver(this);

  // LockWindowState replaces default WindowState of a child.
  wm::WindowState* window_state = LockWindowState::SetLockWindowState(child);
  wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
}

void LockLayoutManager::OnWillRemoveWindowFromLayout(WmWindow* child) {
  child->aura_window()->RemoveObserver(this);
}

void LockLayoutManager::OnWindowRemovedFromLayout(WmWindow* child) {}

void LockLayoutManager::OnChildWindowVisibilityChanged(WmWindow* child,
                                                       bool visible) {}

void LockLayoutManager::SetChildBounds(WmWindow* child,
                                       const gfx::Rect& requested_bounds) {
  wm::WindowState* window_state = child->GetWindowState();
  wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, requested_bounds);
  window_state->OnWMEvent(&event);
}

void LockLayoutManager::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  if (root_window_ == WmWindow::Get(window))
    root_window_ = nullptr;
}

void LockLayoutManager::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  if (root_window_ == WmWindow::Get(window)) {
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

void LockLayoutManager::OnKeyboardClosed() {}

void LockLayoutManager::AdjustWindowsForWorkAreaChange(
    const wm::WMEvent* event) {
  DCHECK(event->type() == wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED ||
         event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);

  for (WmWindow* child : window_->GetChildren())
    child->GetWindowState()->OnWMEvent(event);
}

}  // namespace ash

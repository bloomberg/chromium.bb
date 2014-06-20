// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_window_state.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/lock_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

LockWindowState::LockWindowState(aura::Window* window)
    : current_state_type_(wm::GetWindowState(window)->GetStateType()) {
}

LockWindowState::~LockWindowState() {
}

void LockWindowState::OnWMEvent(wm::WindowState* window_state,
                                const wm::WMEvent* event) {
  aura::Window* window = window_state->window();
  gfx::Rect bounds = window->bounds();

  switch (event->type()) {
    case wm::WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case wm::WM_EVENT_FULLSCREEN:
      UpdateWindow(window_state, wm::WINDOW_STATE_TYPE_FULLSCREEN);
      break;
    case wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case wm::WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_MAXIMIZE:
    case wm::WM_EVENT_CENTER:
    case wm::WM_EVENT_SNAP_LEFT:
    case wm::WM_EVENT_SNAP_RIGHT:
    case wm::WM_EVENT_NORMAL:
    case wm::WM_EVENT_MAXIMIZE:
      UpdateWindow(window_state,
                   GetMaximizedOrCenteredWindowType(window_state));
      return;
    case wm::WM_EVENT_MINIMIZE:
      UpdateWindow(window_state, wm::WINDOW_STATE_TYPE_MINIMIZED);
      return;
    case wm::WM_EVENT_SHOW_INACTIVE:
      return;
    case wm::WM_EVENT_SET_BOUNDS:
      if (window_state->IsMaximized() || window_state->IsFullscreen()) {
        UpdateBounds(window_state);
      } else {
        const ash::wm::SetBoundsEvent* bounds_event =
            static_cast<const ash::wm::SetBoundsEvent*>(event);
        window_state->SetBoundsConstrained(bounds_event->requested_bounds());
      }
      break;
    case wm::WM_EVENT_ADDED_TO_WORKSPACE:
      if (current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED &&
          current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED &&
          current_state_type_ != wm::WINDOW_STATE_TYPE_FULLSCREEN) {
        UpdateWindow(window_state,
                     GetMaximizedOrCenteredWindowType(window_state));
      }
      break;
    case wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED:
    case wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      UpdateBounds(window_state);
      break;
  }
}

wm::WindowStateType LockWindowState::GetType() const {
  return current_state_type_;
}

void LockWindowState::AttachState(wm::WindowState* window_state,
                                  wm::WindowState::State* previous_state) {
  current_state_type_ = previous_state->GetType();

  // Initialize the state to a good preset.
  if (current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_FULLSCREEN) {
    UpdateWindow(window_state,
                 GetMaximizedOrCenteredWindowType(window_state));
  }
}

void LockWindowState::DetachState(wm::WindowState* window_state) {
}

// static
wm::WindowState* LockWindowState::SetLockWindowState(aura::Window* window) {
  scoped_ptr<wm::WindowState::State> lock_state(new LockWindowState(window));
  scoped_ptr<wm::WindowState::State> old_state(
      wm::GetWindowState(window)->SetStateObject(lock_state.Pass()));
  return wm::GetWindowState(window);
}

void LockWindowState::UpdateWindow(wm::WindowState* window_state,
                                   wm::WindowStateType target_state) {
  DCHECK(target_state == wm::WINDOW_STATE_TYPE_MINIMIZED ||
         target_state == wm::WINDOW_STATE_TYPE_MAXIMIZED ||
         (target_state == wm::WINDOW_STATE_TYPE_NORMAL &&
              !window_state->CanMaximize()) ||
         target_state == wm::WINDOW_STATE_TYPE_FULLSCREEN);

  if (target_state == wm::WINDOW_STATE_TYPE_MINIMIZED) {
    if (current_state_type_ == wm::WINDOW_STATE_TYPE_MINIMIZED)
      return;

    current_state_type_ = target_state;
    ::wm::SetWindowVisibilityAnimationType(
        window_state->window(), WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
    window_state->window()->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
    return;
  }

  if (current_state_type_ == target_state) {
    // If the state type did not change, update it accordingly.
    UpdateBounds(window_state);
    return;
  }

  const wm::WindowStateType old_state_type = current_state_type_;
  current_state_type_ = target_state;
  window_state->UpdateWindowShowStateFromStateType();
  window_state->NotifyPreStateTypeChange(old_state_type);
  UpdateBounds(window_state);
  window_state->NotifyPostStateTypeChange(old_state_type);

  if ((window_state->window()->TargetVisibility() ||
      old_state_type == wm::WINDOW_STATE_TYPE_MINIMIZED) &&
      !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }
}

wm::WindowStateType LockWindowState::GetMaximizedOrCenteredWindowType(
      wm::WindowState* window_state) {
  return window_state->CanMaximize() ? wm::WINDOW_STATE_TYPE_MAXIMIZED :
                                       wm::WINDOW_STATE_TYPE_NORMAL;
}

void LockWindowState::UpdateBounds(wm::WindowState* window_state) {
  if (!window_state->IsMaximized() && !window_state->IsFullscreen())
    return;

  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  gfx::Rect keyboard_bounds;

  if (keyboard_controller && !keyboard::IsKeyboardOverscrollEnabled())
    keyboard_bounds = keyboard_controller->current_keyboard_bounds();

  gfx::Rect bounds =
      ScreenUtil::GetDisplayBoundsInParent(window_state->window());
  bounds.set_height(bounds.height() - keyboard_bounds.height());
  window_state->SetBoundsDirect(bounds);
}

}  // namespace ash

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_state.h"

#include "ash/public/cpp/window_state_type.h"
#include "ash/wm/window_animation_types.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {
namespace {

bool IsMinimizedWindowState(const mojom::WindowStateType state_type) {
  return state_type == mojom::WindowStateType::MINIMIZED;
}

}  // namespace

BaseState::BaseState(mojom::WindowStateType initial_state_type)
    : state_type_(initial_state_type) {}
BaseState::~BaseState() = default;

void BaseState::OnWMEvent(WindowState* window_state, const WMEvent* event) {
  if (event->IsWorkspaceEvent()) {
    HandleWorkspaceEvents(window_state, event);
    return;
  }
  if ((window_state->IsTrustedPinned() || window_state->IsPinned()) &&
      (event->type() != WM_EVENT_NORMAL && event->IsTransitionEvent())) {
    // PIN state can be exited only by normal event.
    return;
  }

  if (event->IsCompoundEvent()) {
    HandleCompoundEvents(window_state, event);
    return;
  }

  if (event->IsBoundsEvent()) {
    HandleBoundsEvents(window_state, event);
    return;
  }
  DCHECK(event->IsTransitionEvent());
  HandleTransitionEvents(window_state, event);
}

mojom::WindowStateType BaseState::GetType() const {
  return state_type_;
}

// static
mojom::WindowStateType BaseState::GetStateForTransitionEvent(
    const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_NORMAL:
      return mojom::WindowStateType::NORMAL;
    case WM_EVENT_MAXIMIZE:
      return mojom::WindowStateType::MAXIMIZED;
    case WM_EVENT_MINIMIZE:
      return mojom::WindowStateType::MINIMIZED;
    case WM_EVENT_FULLSCREEN:
      return mojom::WindowStateType::FULLSCREEN;
    case WM_EVENT_SNAP_LEFT:
      return mojom::WindowStateType::LEFT_SNAPPED;
    case WM_EVENT_SNAP_RIGHT:
      return mojom::WindowStateType::RIGHT_SNAPPED;
    case WM_EVENT_SHOW_INACTIVE:
      return mojom::WindowStateType::INACTIVE;
    case WM_EVENT_PIN:
      return mojom::WindowStateType::PINNED;
    case WM_EVENT_TRUSTED_PIN:
      return mojom::WindowStateType::TRUSTED_PINNED;
    default:
      break;
  }
#if !defined(NDEBUG)
  if (event->IsWorkspaceEvent())
    NOTREACHED() << "Can't get the state for Workspace event" << event->type();
  if (event->IsCompoundEvent())
    NOTREACHED() << "Can't get the state for Compound event:" << event->type();
  if (event->IsBoundsEvent())
    NOTREACHED() << "Can't get the state for Bounds event:" << event->type();
#endif
  return mojom::WindowStateType::NORMAL;
}

void BaseState::UpdateMinimizedState(
    WindowState* window_state,
    mojom::WindowStateType previous_state_type) {
  aura::Window* window = window_state->window();
  if (window_state->IsMinimized()) {
    // Save the previous show state so that we can correctly restore it after
    // exiting the minimized mode.
    window->SetProperty(aura::client::kPreMinimizedShowStateKey,
                        ToWindowShowState(previous_state_type));
    ::wm::SetWindowVisibilityAnimationType(
        window, WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

    window->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
  } else if ((window->layer()->GetTargetVisibility() ||
              IsMinimizedWindowState(previous_state_type)) &&
             !window->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window->Show();
    if (IsMinimizedWindowState(previous_state_type) &&
        !window_state->IsMaximizedOrFullscreenOrPinned()) {
      window_state->set_unminimize_to_restore_bounds(false);
    }
  }
}

}  // namespace wm
}  // namespace ash

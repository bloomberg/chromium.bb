// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/event_client_impl.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {

EventClientImpl::EventClientImpl() {
}

EventClientImpl::~EventClientImpl() {
}

bool EventClientImpl::CanProcessEventsWithinSubtree(
    const aura::Window* window) const {
  const aura::Window* root_window = window ? window->GetRootWindow() : NULL;
  if (!root_window ||
      !Shell::GetInstance()->session_state_delegate()->IsUserSessionBlocked()) {
    return true;
  }

  const aura::Window* lock_screen_containers = Shell::GetContainer(
      root_window,
      kShellWindowId_LockScreenContainersContainer);
  const aura::Window* lock_background_containers = Shell::GetContainer(
      root_window,
      kShellWindowId_LockScreenBackgroundContainer);
  const aura::Window* lock_screen_related_containers = Shell::GetContainer(
      root_window,
      kShellWindowId_LockScreenRelatedContainersContainer);
  bool can_process_events = (window->Contains(lock_screen_containers) &&
      window->Contains(lock_background_containers) &&
      window->Contains(lock_screen_related_containers)) ||
      lock_screen_containers->Contains(window) ||
      lock_background_containers->Contains(window) ||
      lock_screen_related_containers->Contains(window);
  if (keyboard::IsKeyboardEnabled()) {
    const aura::Window* virtual_keyboard_container = Shell::GetContainer(
        root_window,
        kShellWindowId_VirtualKeyboardContainer);
    can_process_events |= (window->Contains(virtual_keyboard_container) ||
                           virtual_keyboard_container->Contains(window));
  }
  return can_process_events;
}

ui::EventTarget* EventClientImpl::GetToplevelEventTarget() {
  return Shell::GetInstance();
}

}  // namespace ash

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/event_client_impl.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

EventClientImpl::EventClientImpl() {
}

EventClientImpl::~EventClientImpl() {
}

bool EventClientImpl::CanProcessEventsWithinSubtree(
    const aura::Window* window) const {
  const aura::RootWindow* root_window =
      window ? window->GetRootWindow() : NULL;
  if (Shell::GetInstance()->IsScreenLocked() && root_window) {
    const aura::Window* lock_screen_containers = Shell::GetContainer(
        root_window,
        kShellWindowId_LockScreenContainersContainer);
    const aura::Window* lock_background_containers = Shell::GetContainer(
        root_window,
        kShellWindowId_LockScreenBackgroundContainer);
    const aura::Window* lock_screen_related_containers = Shell::GetContainer(
        root_window,
        kShellWindowId_LockScreenRelatedContainersContainer);
    return (window->Contains(lock_screen_containers) &&
        window->Contains(lock_background_containers) &&
        window->Contains(lock_screen_related_containers)) ||
        lock_screen_containers->Contains(window) ||
        lock_background_containers->Contains(window) ||
        lock_screen_related_containers->Contains(window);
  }
  return true;
}

ui::EventTarget* EventClientImpl::GetToplevelEventTarget() {
  return Shell::GetInstance();
}

}  // namespace internal
}  // namespace ash

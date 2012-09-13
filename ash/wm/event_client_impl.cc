// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/event_client_impl.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

EventClientImpl::EventClientImpl(aura::RootWindow* root_window)
    : root_window_(root_window) {
  aura::client::SetEventClient(root_window_, this);
}

EventClientImpl::~EventClientImpl() {
  aura::client::SetEventClient(root_window_, NULL);
}

bool EventClientImpl::CanProcessEventsWithinSubtree(
    const aura::Window* window) const {
  if (Shell::GetInstance()->IsScreenLocked()) {
    aura::Window* lock_screen_containers = Shell::GetContainer(
        root_window_,
        kShellWindowId_LockScreenContainersContainer);
    aura::Window* lock_background_containers = Shell::GetContainer(
        root_window_,
        kShellWindowId_LockScreenBackgroundContainer);
    aura::Window* lock_screen_related_containers = Shell::GetContainer(
        root_window_,
        kShellWindowId_LockScreenRelatedContainersContainer);
    return lock_screen_containers->Contains(window) ||
        lock_background_containers->Contains(window) ||
        lock_screen_related_containers->Contains(window);
  }
  return true;
}

}  // namespace internal
}  // namespace ash

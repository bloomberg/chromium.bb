// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_locking_manager.h"

#include "ash/session/session_controller.h"
#include "ash/shelf/wm_shelf.h"
#include "ash/shell.h"
#include "ash/wm_shell.h"

namespace ash {

ShelfLockingManager::ShelfLockingManager(WmShelf* shelf)
    : shelf_(shelf), stored_alignment_(shelf->GetAlignment()) {
  DCHECK(shelf_);
  WmShell::Get()->AddLockStateObserver(this);
  SessionController* controller = Shell::Get()->session_controller();
  session_locked_ =
      controller->GetSessionState() != session_manager::SessionState::ACTIVE;
  screen_locked_ = controller->IsScreenLocked();
  controller->AddSessionStateObserver(this);
  Shell::Get()->AddShellObserver(this);
}

ShelfLockingManager::~ShelfLockingManager() {
  WmShell::Get()->RemoveLockStateObserver(this);
  Shell::Get()->session_controller()->RemoveSessionStateObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void ShelfLockingManager::OnLockStateChanged(bool locked) {
  screen_locked_ = locked;
  UpdateLockedState();
}

void ShelfLockingManager::SessionStateChanged(
    session_manager::SessionState state) {
  session_locked_ = state != session_manager::SessionState::ACTIVE;
  UpdateLockedState();
}

void ShelfLockingManager::OnLockStateEvent(EventType event) {
  // Lock when the animation starts, ignoring pre-lock. There's no unlock event.
  screen_locked_ |= event == EVENT_LOCK_ANIMATION_STARTED;
  UpdateLockedState();
}

void ShelfLockingManager::UpdateLockedState() {
  const ShelfAlignment alignment = shelf_->GetAlignment();
  if (is_locked() && alignment != SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    stored_alignment_ = alignment;
    shelf_->SetAlignment(SHELF_ALIGNMENT_BOTTOM_LOCKED);
  } else if (!is_locked() && alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    shelf_->SetAlignment(stored_alignment_);
  }
}

}  // namespace ash

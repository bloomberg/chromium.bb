// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_locking_manager.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_shell.h"

namespace ash {

ShelfLockingManager::ShelfLockingManager(WmShelf* shelf) : shelf_(shelf) {
  WmShell::Get()->AddLockStateObserver(this);
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
  session_locked_ =
      delegate->GetSessionState() != session_manager::SessionState::ACTIVE;
  screen_locked_ = delegate->IsScreenLocked();
  delegate->AddSessionStateObserver(this);
  WmShell::Get()->AddShellObserver(this);
}

ShelfLockingManager::~ShelfLockingManager() {
  WmShell::Get()->RemoveLockStateObserver(this);
  WmShell::Get()->GetSessionStateDelegate()->RemoveSessionStateObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
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

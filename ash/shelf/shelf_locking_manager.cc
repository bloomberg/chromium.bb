// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_locking_manager.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"

namespace ash {

ShelfLockingManager::ShelfLockingManager(Shelf* shelf) : shelf_(shelf) {
  Shell* shell = Shell::GetInstance();
  shell->lock_state_controller()->AddObserver(this);
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
  session_locked_ =
      delegate->GetSessionState() != SessionStateDelegate::SESSION_STATE_ACTIVE;
  screen_locked_ = delegate->IsScreenLocked();
  delegate->AddSessionStateObserver(this);
  WmShell::Get()->AddShellObserver(this);
}

ShelfLockingManager::~ShelfLockingManager() {
  Shell::GetInstance()->lock_state_controller()->RemoveObserver(this);
  WmShell::Get()->GetSessionStateDelegate()->RemoveSessionStateObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
}

void ShelfLockingManager::OnLockStateChanged(bool locked) {
  screen_locked_ = locked;
  UpdateLockedState();
}

void ShelfLockingManager::SessionStateChanged(
    SessionStateDelegate::SessionState state) {
  session_locked_ = state != SessionStateDelegate::SESSION_STATE_ACTIVE;
  UpdateLockedState();
}

void ShelfLockingManager::OnLockStateEvent(EventType event) {
  // Lock when the animation starts, ignoring pre-lock. There's no unlock event.
  screen_locked_ |= event == EVENT_LOCK_ANIMATION_STARTED;
  UpdateLockedState();
}

void ShelfLockingManager::UpdateLockedState() {
  const ShelfAlignment alignment = shelf_->alignment();
  if (is_locked() && alignment != SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    stored_alignment_ = alignment;
    shelf_->SetAlignment(SHELF_ALIGNMENT_BOTTOM_LOCKED);
  } else if (!is_locked() && alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    shelf_->SetAlignment(stored_alignment_);
  }
}

}  // namespace ash

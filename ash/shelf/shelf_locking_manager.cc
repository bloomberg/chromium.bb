// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_locking_manager.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"

namespace ash {

ShelfLockingManager::ShelfLockingManager(Shelf* shelf) : shelf_(shelf) {
  Shell* shell = Shell::GetInstance();
  shell->lock_state_controller()->AddObserver(this);
  SessionStateDelegate* delegate = shell->session_state_delegate();
  session_locked_ =
      delegate->GetSessionState() != SessionStateDelegate::SESSION_STATE_ACTIVE;
  screen_locked_ = delegate->IsScreenLocked();
  delegate->AddSessionStateObserver(this);
  shell->AddShellObserver(this);
}

ShelfLockingManager::~ShelfLockingManager() {
  Shell* shell = Shell::GetInstance();
  shell->lock_state_controller()->RemoveObserver(this);
  shell->session_state_delegate()->RemoveSessionStateObserver(this);
  shell->RemoveShellObserver(this);
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
  // This is only called when locking the screen; there is no unlock event here.
  // It's also called when the screen lock animation begins and should help the
  // shelf appear locked much earlier than ShellObserver::OnLockStateChanged.
  screen_locked_ = true;
  UpdateLockedState();
}

void ShelfLockingManager::UpdateLockedState() {
  const wm::ShelfAlignment alignment = shelf_->alignment();
  if (is_locked() && alignment != wm::SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    stored_alignment_ = alignment;
    shelf_->SetAlignment(wm::SHELF_ALIGNMENT_BOTTOM_LOCKED);
  } else if (!is_locked() && alignment == wm::SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    shelf_->SetAlignment(stored_alignment_);
  }
}

}  // namespace ash

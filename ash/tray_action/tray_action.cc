// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/tray_action/tray_action.h"

#include <utility>

#include "ash/tray_action/tray_action_observer.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"

namespace ash {

TrayAction::TrayAction() : binding_(this) {}

TrayAction::~TrayAction() = default;

void TrayAction::AddObserver(TrayActionObserver* observer) {
  observers_.AddObserver(observer);
}

void TrayAction::RemoveObserver(TrayActionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TrayAction::BindRequest(mojom::TrayActionRequest request) {
  binding_.Bind(std::move(request));
}

mojom::TrayActionState TrayAction::GetLockScreenNoteState() const {
  if (!tray_action_client_)
    return mojom::TrayActionState::kNotAvailable;
  return lock_screen_note_state_;
}

bool TrayAction::IsLockScreenNoteActive() const {
  return GetLockScreenNoteState() == mojom::TrayActionState::kActive;
}

void TrayAction::SetClient(mojom::TrayActionClientPtr tray_action_client,
                           mojom::TrayActionState lock_screen_note_state) {
  mojom::TrayActionState old_lock_screen_note_state = GetLockScreenNoteState();

  tray_action_client_ = std::move(tray_action_client);

  if (tray_action_client_) {
    // Makes sure the state is updated in case the connection is lost.
    tray_action_client_.set_connection_error_handler(
        base::Bind(&TrayAction::SetClient, base::Unretained(this), nullptr,
                   mojom::TrayActionState::kNotAvailable));
    lock_screen_note_state_ = lock_screen_note_state;
  }

  // Setting action handler value can change effective state - notify observers
  // if that was the case.
  if (GetLockScreenNoteState() != old_lock_screen_note_state)
    NotifyLockScreenNoteStateChanged();
}

void TrayAction::UpdateLockScreenNoteState(mojom::TrayActionState state) {
  if (state == lock_screen_note_state_)
    return;

  lock_screen_note_state_ = state;

  // If the client is not set, the effective state has not changed, so no need
  // to notify observers of a state change.
  if (tray_action_client_)
    NotifyLockScreenNoteStateChanged();
}

void TrayAction::RequestNewLockScreenNote(mojom::LockScreenNoteOrigin origin) {
  if (GetLockScreenNoteState() != mojom::TrayActionState::kAvailable)
    return;

  // An action state can be kAvailable only if |tray_action_client_| is set.
  DCHECK(tray_action_client_);
  tray_action_client_->RequestNewLockScreenNote(origin);
}

void TrayAction::CloseLockScreenNote(mojom::CloseLockScreenNoteReason reason) {
  if (tray_action_client_)
    tray_action_client_->CloseLockScreenNote(reason);
}

void TrayAction::FlushMojoForTesting() {
  if (tray_action_client_)
    tray_action_client_.FlushForTesting();
}

void TrayAction::NotifyLockScreenNoteStateChanged() {
  for (auto& observer : observers_)
    observer.OnLockScreenNoteStateChanged(GetLockScreenNoteState());
}

}  // namespace ash

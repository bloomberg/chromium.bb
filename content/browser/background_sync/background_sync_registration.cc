// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_registration.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"

namespace content {

const BackgroundSyncRegistration::RegistrationId
    BackgroundSyncRegistration::kInvalidRegistrationId = -1;

const BackgroundSyncRegistration::RegistrationId
    BackgroundSyncRegistration::kInitialId = 0;

BackgroundSyncRegistration::BackgroundSyncRegistration() = default;
BackgroundSyncRegistration::~BackgroundSyncRegistration() = default;

bool BackgroundSyncRegistration::Equals(
    const BackgroundSyncRegistration& other) const {
  return options_.Equals(other.options_);
}

bool BackgroundSyncRegistration::IsValid() const {
  return id_ != kInvalidRegistrationId;
}

void BackgroundSyncRegistration::AddFinishedCallback(
    const StateCallback& callback) {
  DCHECK(!HasCompleted());
  notify_finished_callbacks_.push_back(callback);
}

void BackgroundSyncRegistration::RunFinishedCallbacks() {
  DCHECK(HasCompleted());

  for (auto& callback : notify_finished_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, sync_state_));
  }

  notify_finished_callbacks_.clear();
}

bool BackgroundSyncRegistration::HasCompleted() const {
  switch (sync_state_) {
    case BACKGROUND_SYNC_STATE_PENDING:
    case BACKGROUND_SYNC_STATE_FIRING:
    case BACKGROUND_SYNC_STATE_REREGISTERED_WHILE_FIRING:
    case BACKGROUND_SYNC_STATE_UNREGISTERED_WHILE_FIRING:
      return false;
    case BACKGROUND_SYNC_STATE_FAILED:
    case BACKGROUND_SYNC_STATE_SUCCESS:
    case BACKGROUND_SYNC_STATE_UNREGISTERED:
      return true;
  }
  NOTREACHED();
  return false;
}

bool BackgroundSyncRegistration::IsFiring() const {
  switch (sync_state_) {
    case BACKGROUND_SYNC_STATE_FIRING:
    case BACKGROUND_SYNC_STATE_REREGISTERED_WHILE_FIRING:
    case BACKGROUND_SYNC_STATE_UNREGISTERED_WHILE_FIRING:
      return true;
    case BACKGROUND_SYNC_STATE_PENDING:
    case BACKGROUND_SYNC_STATE_FAILED:
    case BACKGROUND_SYNC_STATE_SUCCESS:
    case BACKGROUND_SYNC_STATE_UNREGISTERED:
      return false;
  }
  NOTREACHED();
  return false;
}

void BackgroundSyncRegistration::SetUnregisteredState() {
  DCHECK(!HasCompleted());

  bool is_firing = IsFiring();

  sync_state_ = is_firing ? BACKGROUND_SYNC_STATE_UNREGISTERED_WHILE_FIRING
                          : BACKGROUND_SYNC_STATE_UNREGISTERED;

  if (!is_firing) {
    // If the registration is currently firing then wait to run
    // RunFinishedCallbacks until after it has finished as it might
    // change state to SUCCESS first.
    RunFinishedCallbacks();
  }
}

}  // namespace content

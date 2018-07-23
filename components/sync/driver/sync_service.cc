// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service.h"

namespace syncer {

SyncSetupInProgressHandle::SyncSetupInProgressHandle(base::Closure on_destroy)
    : on_destroy_(on_destroy) {}

SyncSetupInProgressHandle::~SyncSetupInProgressHandle() {
  on_destroy_.Run();
}

bool SyncService::CanSyncStart() const {
  return GetDisableReasons() == DISABLE_REASON_NONE;
}

bool SyncService::IsSyncAllowed() const {
  return !HasDisableReason(DISABLE_REASON_PLATFORM_OVERRIDE) &&
         !HasDisableReason(DISABLE_REASON_ENTERPRISE_POLICY);
}

bool SyncService::IsEngineInitialized() const {
  switch (GetState()) {
    case State::DISABLED:
    case State::WAITING_FOR_START_REQUEST:
    case State::START_DEFERRED:
    case State::INITIALIZING:
      return false;
    case State::PENDING_DESIRED_CONFIGURATION:
    case State::CONFIGURING:
    case State::ACTIVE:
      return true;
  }
  NOTREACHED();
  return false;
}

bool SyncService::IsSyncActive() const {
  switch (GetState()) {
    case State::DISABLED:
    case State::WAITING_FOR_START_REQUEST:
    case State::START_DEFERRED:
    case State::INITIALIZING:
    case State::PENDING_DESIRED_CONFIGURATION:
      return false;
    case State::CONFIGURING:
    case State::ACTIVE:
      return true;
  }
  NOTREACHED();
  return false;
}

bool SyncService::IsFirstSetupInProgress() const {
  return !IsFirstSetupComplete() && IsSetupInProgress();
}

bool SyncService::HasUnrecoverableError() const {
  return HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR);
}

}  // namespace syncer

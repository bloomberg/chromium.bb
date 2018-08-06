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

bool SyncService::IsSyncFeatureEnabled() const {
  return GetDisableReasons() == DISABLE_REASON_NONE && IsFirstSetupComplete();
}

bool SyncService::CanSyncStart() const {
  return GetDisableReasons() == DISABLE_REASON_NONE;
}

bool SyncService::IsEngineInitialized() const {
  switch (GetTransportState()) {
    case TransportState::DISABLED:
    case TransportState::WAITING_FOR_START_REQUEST:
    case TransportState::START_DEFERRED:
    case TransportState::INITIALIZING:
      return false;
    case TransportState::PENDING_DESIRED_CONFIGURATION:
    case TransportState::CONFIGURING:
    case TransportState::ACTIVE:
      return true;
  }
  NOTREACHED();
  return false;
}

bool SyncService::IsSyncActive() const {
  if (!IsSyncFeatureEnabled()) {
    return false;
  }
  switch (GetTransportState()) {
    case TransportState::DISABLED:
    case TransportState::WAITING_FOR_START_REQUEST:
    case TransportState::START_DEFERRED:
    case TransportState::INITIALIZING:
    case TransportState::PENDING_DESIRED_CONFIGURATION:
      return false;
    case TransportState::CONFIGURING:
    case TransportState::ACTIVE:
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

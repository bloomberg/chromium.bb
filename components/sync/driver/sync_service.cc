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
  int disable_reasons = GetDisableReasons();
  // An unrecoverable error is currently *not* considered a start-preventing
  // disable reason, because it occurs after Sync has already started.
  // TODO(crbug.com/839834): Consider changing this, since Sync shuts down and
  // won't start up again after an unrecoverable error.
  disable_reasons = disable_reasons & ~DISABLE_REASON_UNRECOVERABLE_ERROR;
  return disable_reasons == DISABLE_REASON_NONE;
}

bool SyncService::IsSyncAllowed() const {
  return !HasDisableReason(DISABLE_REASON_PLATFORM_OVERRIDE) &&
         !HasDisableReason(DISABLE_REASON_ENTERPRISE_POLICY);
}

bool SyncService::HasUnrecoverableError() const {
  return HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR);
}

}  // namespace syncer

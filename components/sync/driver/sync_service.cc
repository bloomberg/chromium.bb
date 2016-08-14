// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service.h"

#include "components/sync/core/sync_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace sync_driver {

SyncSetupInProgressHandle::SyncSetupInProgressHandle(base::Closure on_destroy)
    : on_destroy_(on_destroy) {}

SyncSetupInProgressHandle::~SyncSetupInProgressHandle() {
  on_destroy_.Run();
}

SyncService::SyncTokenStatus::SyncTokenStatus()
    : connection_status(syncer::CONNECTION_NOT_ATTEMPTED),
      last_get_token_error(GoogleServiceAuthError::AuthErrorNone()) {}

}  // namespace sync_driver

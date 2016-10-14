// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service.h"

#include "components/sync/engine/sync_manager.h"

namespace syncer {

SyncSetupInProgressHandle::SyncSetupInProgressHandle(base::Closure on_destroy)
    : on_destroy_(on_destroy) {}

SyncSetupInProgressHandle::~SyncSetupInProgressHandle() {
  on_destroy_.Run();
}

SyncService::SyncTokenStatus::SyncTokenStatus()
    : connection_status(CONNECTION_NOT_ATTEMPTED),
      last_get_token_error(GoogleServiceAuthError::AuthErrorNone()) {}

}  // namespace syncer

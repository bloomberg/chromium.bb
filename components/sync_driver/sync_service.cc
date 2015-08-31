// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_service.h"

#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/internal_api/public/sync_manager.h"

namespace sync_driver {

SyncService::SyncTokenStatus::SyncTokenStatus()
    : connection_status(syncer::CONNECTION_NOT_ATTEMPTED),
      last_get_token_error(GoogleServiceAuthError::AuthErrorNone()) {}
SyncService::SyncTokenStatus::~SyncTokenStatus() {}

}

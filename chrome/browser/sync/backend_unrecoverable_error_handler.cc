// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"

#include "chrome/browser/sync/backend_unrecoverable_error_handler.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

BackendUnrecoverableErrorHandler::BackendUnrecoverableErrorHandler(
    const syncer::WeakHandle<ProfileSyncService>& service) : service_(service) {
}

BackendUnrecoverableErrorHandler::~BackendUnrecoverableErrorHandler() {
}

void BackendUnrecoverableErrorHandler::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  service_.Call(FROM_HERE,
                &ProfileSyncService::OnUnrecoverableError,
                from_here,
                message);
}

}  // namespace browser_sync

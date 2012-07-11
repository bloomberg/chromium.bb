// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_BACKEND_UNRECOVERABLE_ERROR_HANDLER_H_
#define CHROME_BROWSER_SYNC_BACKEND_UNRECOVERABLE_ERROR_HANDLER_H_

#include <string>

#include "base/location.h"
#include "base/memory/weak_ptr.h"

#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/util/weak_handle.h"

class ProfileSyncService;
namespace browser_sync {

class BackendUnrecoverableErrorHandler
    : public syncer::UnrecoverableErrorHandler {
 public:
  BackendUnrecoverableErrorHandler(
      const syncer::WeakHandle<ProfileSyncService>& service);
  virtual ~BackendUnrecoverableErrorHandler();
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message) OVERRIDE;

 private:
  syncer::WeakHandle<ProfileSyncService> service_;
};
}  // namespace browser_sync
#endif  // CHROME_BROWSER_SYNC_BACKEND_UNRECOVERABLE_ERROR_HANDLER_H_


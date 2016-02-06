// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_registration_handle.h"

#include "content/browser/background_sync/background_sync_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundSyncRegistrationHandle::~BackgroundSyncRegistrationHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (IsValid() && background_sync_manager_)
    background_sync_manager_->ReleaseRegistrationHandle(handle_id_);
}

void BackgroundSyncRegistrationHandle::Unregister(
    int64_t sw_registration_id,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValid());
  DCHECK(background_sync_manager_);

  background_sync_manager_->Unregister(sw_registration_id, handle_id_,
                                       callback);
}

bool BackgroundSyncRegistrationHandle::IsValid() const {
  return registration_ != nullptr;
}

BackgroundSyncRegistrationHandle::BackgroundSyncRegistrationHandle(
    base::WeakPtr<BackgroundSyncManager> background_sync_manager,
    HandleId handle_id)
    : background_sync_manager_(background_sync_manager),
      handle_id_(handle_id),
      registration_(
          background_sync_manager_->GetRegistrationForHandle(handle_id_)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_manager_);
}

}  // namespace content

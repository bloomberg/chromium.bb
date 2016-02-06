// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_HANDLE_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_HANDLE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_registration.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/common/content_export.h"

namespace content {

class BackgroundSyncManager;

// Handle to BackgroundSyncRegistration that is exposed to clients. Each
// BackgroundSyncRegistrationHandle is given a unique handle id (by the
// BackgroundSyncManager) which is released at destruction.
// BackgroundSyncRegistrationHandle objects must not be used (but may be
// destroyed) after the BackgroundSyncManager has been deleted.
class CONTENT_EXPORT BackgroundSyncRegistrationHandle {
 public:
  using HandleId = int64_t;
  using StatusCallback = base::Callback<void(BackgroundSyncStatus)>;
  using StatusAndStateCallback =
      base::Callback<void(BackgroundSyncStatus, BackgroundSyncState)>;

  ~BackgroundSyncRegistrationHandle();

  const BackgroundSyncRegistrationOptions* options() const {
    DCHECK(background_sync_manager_);
    return registration_->options();
  }

  BackgroundSyncState sync_state() const {
    DCHECK(background_sync_manager_);
    return registration_->sync_state();
  }

  // Unregisters the background sync registration.  Calls |callback|
  // with BACKGROUND_SYNC_STATUS_OK if it succeeds.
  void Unregister(int64_t service_worker_id, const StatusCallback& callback);

  // Returns true if the handle is backed by a BackgroundSyncRegistration in the
  // BackgroundSyncManager.
  bool IsValid() const;

  HandleId handle_id() const { return handle_id_; }

 private:
  friend class BackgroundSyncManager;

  BackgroundSyncRegistrationHandle(
      base::WeakPtr<BackgroundSyncManager> background_sync_manager,
      HandleId handle_id);

  BackgroundSyncRegistration* registration() {
    DCHECK(background_sync_manager_);
    return registration_;
  }

  // The BackgroundSyncManager is expected to remain alive for all operations
  // except for possibly at destruction.
  base::WeakPtr<BackgroundSyncManager> background_sync_manager_;

  // Each BackgroundSyncRegistrationHandle is assigned a unique handle id.
  // The BackgroundSyncManager maps the id to an internal pointer.
  HandleId handle_id_;

  // This is owned by background_sync_manager_ and is valid until handle_id_ is
  // released in the destructor or background_sync_manager_ has been destroyed.
  BackgroundSyncRegistration* registration_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncRegistrationHandle);
};

}  // namespace

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_HANDLE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_BASE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_BASE_H_

#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "sync/internal_api/public/base/model_type.h"

namespace content {
class BrowserContext;
}

// API for ProfileSyncService.
class ProfileSyncServiceBase {
 public:
  // Retrieve the sync service to use in the given context.
  // Returns NULL if sync is not enabled for the context.
  static ProfileSyncServiceBase* FromBrowserContext(
      content::BrowserContext* context);

  typedef ProfileSyncServiceObserver Observer;

  virtual ~ProfileSyncServiceBase() {}

  // Whether sync is enabled by user or not. This does not necessarily mean
  // that sync is currently running (due to delayed startup, unrecoverable
  // errors, or shutdown). See SyncActive below for checking whether sync
  // is actually running.
  virtual bool HasSyncSetupCompleted() const = 0;

  // Returns true if sync is fully initialized and active. This implies that
  // an initial configuration has successfully completed, although there may
  // be datatype specific, auth, or other transient errors. To see which
  // datetypes are actually syncing, see GetActiveTypes() below.
  // Note that if sync is in backup or rollback mode, SyncActive() will be
  // false.
  virtual bool SyncActive() const = 0;

  // Get the set of current active data types (those chosen or configured by
  // the user which have not also encountered a runtime error).
  // Note that if the Sync engine is in the middle of a configuration, this
  // will the the empty set. Once the configuration completes the set will
  // be updated.
  virtual syncer::ModelTypeSet GetActiveDataTypes() const = 0;

  // Adds/removes an observer. ProfileSyncServiceBase does not take
  // ownership of the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true if |observer| has already been added as an observer.
  virtual bool HasObserver(const Observer* observer) const = 0;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_BASE_H_

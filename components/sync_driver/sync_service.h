// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_

#include "base/macros.h"
#include "components/sync_driver/sync_service_observer.h"
#include "sync/internal_api/public/base/model_type.h"

namespace sync_driver {

class SyncService {
 public:
  virtual ~SyncService() {}

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

  // Adds/removes an observer. SyncService does not take ownership of the
  // observer.
  virtual void AddObserver(SyncServiceObserver* observer) = 0;
  virtual void RemoveObserver(SyncServiceObserver* observer) = 0;

  // Returns true if |observer| has already been added as an observer.
  virtual bool HasObserver(const SyncServiceObserver* observer) const = 0;

 protected:
  SyncService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncService);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_SYNCABLE_SERVICE_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_SYNCABLE_SERVICE_H__

#include "base/memory/scoped_ptr.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

namespace syncer {
class SyncErrorFactory;
}  // namespace syncer

class PasswordStore;

class PasswordSyncableService : public syncer::SyncableService {
 public:
  PasswordSyncableService();
  virtual ~PasswordSyncableService();

  // syncer::SyncableServiceImplementations
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

 private:
  scoped_ptr<syncer::SyncErrorFactory> sync_error_factory_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_SYNCABLE_SERVICE_H__


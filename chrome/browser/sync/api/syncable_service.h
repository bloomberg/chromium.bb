// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_API_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_SYNC_API_SYNCABLE_SERVICE_H_
#pragma once

#include <vector>

#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/api/sync_change_processor.h"
#include "chrome/browser/sync/api/sync_data.h"

class SyncData;

typedef std::vector<SyncData> SyncDataList;

class SyncableService : public SyncChangeProcessor {
 public:
  // Informs the service to begin syncing the specified synced datatype |type|.
  // The service should then merge |initial_sync_data| into it's local data,
  // calling |sync_processor|'s ProcessSyncChanges as necessary to reconcile the
  // two. After this, the SyncableService's local data should match the server
  // data, and the service should be ready to receive and process any further
  // SyncChange's as they occur.
  virtual bool MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      SyncChangeProcessor* sync_processor) = 0;

  // Stop syncing the specified type and reset state.
  virtual void StopSyncing(syncable::ModelType type) = 0;

  // Fills a list of SyncData from the local data. This should create an up
  // to date representation of the SyncableService's view of that datatype, and
  // should match/be a subset of the server's view of that datatype.
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const = 0;

  // SyncChangeProcessor interface.
  // Process a list of new SyncChanges and update the local data as necessary.
  virtual void ProcessSyncChanges(const SyncChangeList& change_list) = 0;

 protected:
  virtual ~SyncableService();
};

#endif  // CHROME_BROWSER_SYNC_API_SYNCABLE_SERVICE_H_

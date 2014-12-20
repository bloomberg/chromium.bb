// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_H_
#define COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/syncable_service.h"

namespace wifi_sync {

// KeyedService that synchronizes WiFi credentials between local settings,
// and Chrome Sync.
//
// This service does not necessarily own the storage for WiFi
// credentials. In particular, on ChromeOS, WiFi credential storage is
// managed by the ChromeOS connection manager ("Shill").
class WifiCredentialSyncableService
    : public syncer::SyncableService, public KeyedService {
 public:
  WifiCredentialSyncableService();
  ~WifiCredentialSyncableService() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& caller_location,
      const syncer::SyncChangeList& change_list) override;

 private:
  // The syncer::ModelType that this SyncableService processes and
  // generates updates for.
  static const syncer::ModelType kModelType;

  // Our SyncChangeProcessor instance. Used to push changes into
  // Chrome Sync.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  DISALLOW_COPY_AND_ASSIGN(WifiCredentialSyncableService);
};

}  // namespace wifi_sync

#endif  // COMPONENTS_WIFI_SYNC_WIFI_CREDENTIAL_SYNCABLE_SERVICE_H_

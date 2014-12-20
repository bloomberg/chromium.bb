// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi_sync/wifi_credential_syncable_service.h"

#include "base/logging.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"

namespace wifi_sync {

const syncer::ModelType WifiCredentialSyncableService::kModelType =
    syncer::WIFI_CREDENTIALS;

WifiCredentialSyncableService::WifiCredentialSyncableService() {
}

WifiCredentialSyncableService::~WifiCredentialSyncableService() {
}

// Implementation of syncer::SyncableService API.
syncer::SyncMergeResult WifiCredentialSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> /* error_handler */) {
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK_EQ(kModelType, type);

  sync_processor_ = sync_processor.Pass();

  // TODO(quiche): Update local WiFi configuration.
  // TODO(quiche): Notify upper layers that sync is ready.
  NOTIMPLEMENTED();

  return syncer::SyncMergeResult(type);
}

void WifiCredentialSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(kModelType, type);
  sync_processor_.reset();
}

syncer::SyncDataList WifiCredentialSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(kModelType, type);
  NOTIMPLEMENTED();
  return syncer::SyncDataList();
}

syncer::SyncError WifiCredentialSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& /* caller_location */,
    const syncer::SyncChangeList& change_list) {
  NOTIMPLEMENTED();
  return syncer::SyncError();
}

}  // namespace wifi_sync

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_syncable_service.h"

#include "base/location.h"
#include "sync/api/sync_error_factory.h"

PasswordSyncableService::PasswordSyncableService() {
}

PasswordSyncableService::~PasswordSyncableService() {}

syncer::SyncMergeResult
PasswordSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  syncer::SyncMergeResult merge_result(type);
  sync_error_factory_ = sync_error_factory.Pass();
  sync_processor_ = sync_processor.Pass();

  merge_result.set_error(sync_error_factory->CreateAndUploadError(
      FROM_HERE,
      "Password Syncable Service Not Implemented."));
  return merge_result;
}

void PasswordSyncableService::StopSyncing(syncer::ModelType type) {
}

syncer::SyncDataList PasswordSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  syncer::SyncDataList sync_data;
  return sync_data;
}

syncer::SyncError PasswordSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::UNRECOVERABLE_ERROR,
                          "Password Syncable Service Not Implemented.",
                          syncer::PASSWORDS);
  return error;
}


// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"

namespace sync_file_system {
namespace drive_backend {

RemoteToLocalSyncer::RemoteToLocalSyncer(SyncEngineContext* sync_context)
    : sync_context_(sync_context),
      weak_ptr_factory_(this) {
}

RemoteToLocalSyncer::~RemoteToLocalSyncer() {
  NOTIMPLEMENTED();
}

void RemoteToLocalSyncer::Run(const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

drive::DriveServiceInterface* RemoteToLocalSyncer::drive_service() {
  return sync_context_->GetDriveService();
}

MetadataDatabase* RemoteToLocalSyncer::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

RemoteChangeProcessor* RemoteToLocalSyncer::remote_change_processor() {
  DCHECK(sync_context_->GetRemoteChangeProcessor());
  return sync_context_->GetRemoteChangeProcessor();
}

}  // namespace drive_backend
}  // namespace sync_file_system

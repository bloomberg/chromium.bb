// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"

namespace sync_file_system {
namespace drive_backend {

SyncEngineContext::SyncEngineContext(
    drive::DriveServiceInterface* drive_service,
    drive::DriveUploaderInterface* drive_uploader,
    base::SequencedTaskRunner* task_runner)
    : drive_service_(drive_service),
      drive_uploader_(drive_uploader),
      remote_change_processor_(NULL),
      task_runner_(task_runner) {}

SyncEngineContext::~SyncEngineContext() {}

drive::DriveServiceInterface* SyncEngineContext::GetDriveService() {
  return drive_service_;
}

drive::DriveUploaderInterface* SyncEngineContext::GetDriveUploader() {
  return drive_uploader_;
}

MetadataDatabase* SyncEngineContext::GetMetadataDatabase() {
  return metadata_database_.get();
}

scoped_ptr<MetadataDatabase> SyncEngineContext::PassMetadataDatabase() {
  return metadata_database_.Pass();
}

RemoteChangeProcessor* SyncEngineContext::GetRemoteChangeProcessor() {
  return remote_change_processor_;
}

base::SequencedTaskRunner* SyncEngineContext::GetBlockingTaskRunner() {
  return task_runner_.get();
}

void SyncEngineContext::SetMetadataDatabase(
    scoped_ptr<MetadataDatabase> metadata_database) {
  if (metadata_database)
    metadata_database_ = metadata_database.Pass();
}

void SyncEngineContext::SetRemoteChangeProcessor(
    RemoteChangeProcessor* remote_change_processor) {
  remote_change_processor_ = remote_change_processor;
}

}  // namespace drive_backend
}  // namespace sync_file_system

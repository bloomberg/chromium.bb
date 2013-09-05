// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

namespace sync_file_system {
namespace drive_backend {

SyncEngineInitializer::SyncEngineInitializer(
    base::SequencedTaskRunner* task_runner,
    drive::DriveAPIService* drive_api,
    const base::FilePath& database_path)
    : task_runner_(task_runner),
      drive_api_(drive_api),
      database_path_(database_path),
      weak_ptr_factory_(this) {
}

SyncEngineInitializer::~SyncEngineInitializer() {
  NOTIMPLEMENTED();
}

void SyncEngineInitializer::Run(const SyncStatusCallback& callback) {
  MetadataDatabase::Create(
      task_runner_.get(), database_path_,
      base::Bind(&SyncEngineInitializer::DidCreateMetadataDatabase,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void SyncEngineInitializer::DidCreateMetadataDatabase(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    scoped_ptr<MetadataDatabase> instance) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  metadata_database_ = instance.Pass();

  // TODO(tzik): Set up sync root and populate database with initial folder
  // tree.

  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

}  // namespace drive_backend
}  // namespace sync_file_system

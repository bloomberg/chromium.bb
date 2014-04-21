// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

class MetadataDatabase;

class SyncEngineContext {
 public:
  SyncEngineContext(
      drive::DriveServiceInterface* drive_service,
      drive::DriveUploaderInterface* drive_uploader,
      base::SequencedTaskRunner* task_runner);
  ~SyncEngineContext();

  drive::DriveServiceInterface* GetDriveService();
  drive::DriveUploaderInterface* GetDriveUploader();
  MetadataDatabase* GetMetadataDatabase();
  RemoteChangeProcessor* GetRemoteChangeProcessor();
  base::SequencedTaskRunner* GetBlockingTaskRunner();

  void SetMetadataDatabase(scoped_ptr<MetadataDatabase> metadata_database);
  void SetRemoteChangeProcessor(RemoteChangeProcessor* remote_change_processor);

  scoped_ptr<MetadataDatabase> PassMetadataDatabase();

 private:
  drive::DriveServiceInterface* drive_service_;
  drive::DriveUploaderInterface* drive_uploader_;
  RemoteChangeProcessor* remote_change_processor_;

  scoped_ptr<MetadataDatabase> metadata_database_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineContext);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_

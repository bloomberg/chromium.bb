// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace sync_file_system {

class RemoteChangeProcessor;
class TaskLogger;

namespace drive_backend {

class MetadataDatabase;

class SyncEngineContext {
 public:
  SyncEngineContext(
      scoped_ptr<drive::DriveServiceInterface> drive_service,
      scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
      TaskLogger* task_logger,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner,
      base::SequencedTaskRunner* file_task_runner);
  ~SyncEngineContext();

  void SetMetadataDatabase(scoped_ptr<MetadataDatabase> metadata_database);
  void SetRemoteChangeProcessor(
      RemoteChangeProcessor* remote_change_processor);

  drive::DriveServiceInterface* GetDriveService();
  drive::DriveUploaderInterface* GetDriveUploader();
  base::WeakPtr<TaskLogger> GetTaskLogger();
  MetadataDatabase* GetMetadataDatabase();
  RemoteChangeProcessor* GetRemoteChangeProcessor();
  base::SingleThreadTaskRunner* GetUITaskRunner();
  base::SequencedTaskRunner* GetWorkerTaskRunner();
  base::SequencedTaskRunner* GetFileTaskRunner();

  scoped_ptr<MetadataDatabase> PassMetadataDatabase();

  void DetachFromSequence();

 private:
  friend class DriveBackendSyncTest;

  scoped_ptr<drive::DriveServiceInterface> drive_service_;
  scoped_ptr<drive::DriveUploaderInterface> drive_uploader_;
  base::WeakPtr<TaskLogger> task_logger_;
  RemoteChangeProcessor* remote_change_processor_;  // Not owned.

  scoped_ptr<MetadataDatabase> metadata_database_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineContext);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_

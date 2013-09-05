// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_INITIALIZER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_INITIALIZER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_task.h"

namespace drive {
class DriveAPIService;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;

class SyncEngineInitializer : public SyncTask {
 public:
  SyncEngineInitializer(base::SequencedTaskRunner* task_runner,
                        drive::DriveAPIService* drive_api,
                        const base::FilePath& database_path);
  virtual ~SyncEngineInitializer();
  virtual void Run(const SyncStatusCallback& callback) OVERRIDE;

 private:
  void DidCreateMetadataDatabase(const SyncStatusCallback& callback,
                                 SyncStatusCode status,
                                 scoped_ptr<MetadataDatabase> instance);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  drive::DriveAPIService* drive_api_;
  base::FilePath database_path_;

  scoped_ptr<MetadataDatabase> metadata_database_;

  base::WeakPtrFactory<SyncEngineInitializer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineInitializer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_INITIALIZER_H_

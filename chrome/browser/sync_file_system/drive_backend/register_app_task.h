// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REGISTER_APP_TASK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REGISTER_APP_TASK_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_task.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class ResourceEntry;
class ResourceList;
}

namespace sync_file_system {
namespace drive_backend {

class FileTracker;
class MetadataDatabase;
class SyncEngineContext;
class TrackerSet;

class RegisterAppTask : public SyncTask {
 public:
  RegisterAppTask(SyncEngineContext* sync_context, const std::string& app_id);
  virtual ~RegisterAppTask();

  virtual void Run(const SyncStatusCallback& callback) OVERRIDE;

 private:
  void CreateAppRootFolder(const SyncStatusCallback& callback);
  void DidCreateAppRootFolder(const SyncStatusCallback& callback,
                              int64 change_id,
                              google_apis::GDataErrorCode error,
                              scoped_ptr<google_apis::ResourceEntry> entry);
  void DidUpdateDatabase(const SyncStatusCallback& callback,
                         const std::string& file_id,
                         SyncStatusCode status);
  void DidPrepareForRegister(const SyncStatusCallback& callback,
                             SyncStatusCode status);
  bool FilterCandidates(const TrackerSet& trackers,
                        FileTracker* candidate);
  void RegisterAppIntoDatabase(const FileTracker& tracker,
                               const SyncStatusCallback& callback);

  MetadataDatabase* metadata_database();
  drive::DriveServiceInterface* drive_service();

  SyncEngineContext* sync_context_;  // Not owned.

  int create_folder_retry_count_;
  std::string app_id_;

  base::WeakPtrFactory<RegisterAppTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAppTask);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REGISTER_APP_TASK_H_

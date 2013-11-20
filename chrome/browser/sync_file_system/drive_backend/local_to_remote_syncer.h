// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_task.h"

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

class FileTracker;
class MetadataDatabase;
class SyncEngineContext;

class LocalToRemoteSyncer : public SyncTask {
 public:
  LocalToRemoteSyncer(SyncEngineContext* sync_context,
                      const FileChange& local_change,
                      const base::FilePath& local_path,
                      const SyncFileMetadata& local_metadata,
                      const fileapi::FileSystemURL& url);
  virtual ~LocalToRemoteSyncer();
  virtual void Run(const SyncStatusCallback& callback) OVERRIDE;

 private:
  void HandleMissingRemoteFile(const SyncStatusCallback& callback);
  void HandleConflict(const SyncStatusCallback& callback);
  void HandleExistingRemoteFile(const SyncStatusCallback& callback);

  void DidDeleteRemoteFile(const SyncStatusCallback& callback,
                           google_apis::GDataErrorCode error);

  bool PopulateRemoteParentFolder();

  drive::DriveServiceInterface* drive_service();
  drive::DriveUploaderInterface* drive_uploader();
  MetadataDatabase* metadata_database();

  SyncEngineContext* sync_context_;  // Not owned.

  FileChange local_change_;
  base::FilePath local_path_;
  SyncFileMetadata local_metadata_;
  fileapi::FileSystemURL url_;

  scoped_ptr<FileTracker> remote_file_tracker_;
  scoped_ptr<FileTracker> remote_parent_folder_tracker_;

  base::WeakPtrFactory<LocalToRemoteSyncer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalToRemoteSyncer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_

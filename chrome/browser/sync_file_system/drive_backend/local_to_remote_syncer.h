// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace google_apis {
class FileResource;
class ResourceList;
}

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

class FileDetails;
class FileTracker;
class FolderCreator;
class MetadataDatabase;
class SyncEngineContext;

class LocalToRemoteSyncer : public SyncTask {
 public:
  typedef base::Callback<void(scoped_ptr<SyncTaskToken>)> Continuation;

  LocalToRemoteSyncer(SyncEngineContext* sync_context,
                      const SyncFileMetadata& local_metadata,
                      const FileChange& local_change,
                      const base::FilePath& local_path,
                      const fileapi::FileSystemURL& url);
  virtual ~LocalToRemoteSyncer();
  virtual void RunPreflight(scoped_ptr<SyncTaskToken> token) OVERRIDE;

  const fileapi::FileSystemURL& url() const { return url_; }
  const base::FilePath& target_path() const { return target_path_; }
  SyncAction sync_action() const { return sync_action_; }
  bool needs_remote_change_listing() const {
    return needs_remote_change_listing_;
  }

 private:
  void MoveToBackground(const Continuation& continuation,
                        scoped_ptr<SyncTaskToken> token);
  void ContinueAsBackgroundTask(const Continuation& continuation,
                                scoped_ptr<SyncTaskToken> token);
  void SyncCompleted(scoped_ptr<SyncTaskToken> token,
                     SyncStatusCode status);

  void HandleConflict(scoped_ptr<SyncTaskToken> token);
  void HandleExistingRemoteFile(scoped_ptr<SyncTaskToken> token);

  void UpdateTrackerForReusedFolder(const FileDetails& details,
                                    scoped_ptr<SyncTaskToken> token);

  void DeleteRemoteFile(scoped_ptr<SyncTaskToken> token);
  void DidDeleteRemoteFile(scoped_ptr<SyncTaskToken> token,
                           google_apis::GDataErrorCode error);

  void UploadExistingFile(scoped_ptr<SyncTaskToken> token);
  void DidGetMD5ForUpload(scoped_ptr<SyncTaskToken> token,
                          const std::string& local_file_md5);
  void DidUploadExistingFile(scoped_ptr<SyncTaskToken> token,
                             google_apis::GDataErrorCode error,
                             const GURL&,
                             scoped_ptr<google_apis::FileResource>);
  void DidUpdateDatabaseForUploadExistingFile(
      scoped_ptr<SyncTaskToken> token,
      SyncStatusCode status);
  void UpdateRemoteMetadata(const std::string& file_id,
                            scoped_ptr<SyncTaskToken> token);
  void DidGetRemoteMetadata(const std::string& file_id,
                            scoped_ptr<SyncTaskToken> token,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::FileResource> entry);

  void UploadNewFile(scoped_ptr<SyncTaskToken> token);
  void DidUploadNewFile(scoped_ptr<SyncTaskToken> token,
                        google_apis::GDataErrorCode error,
                        const GURL& upload_location,
                        scoped_ptr<google_apis::FileResource> entry);

  void CreateRemoteFolder(scoped_ptr<SyncTaskToken> token);
  void DidCreateRemoteFolder(scoped_ptr<SyncTaskToken> token,
                             const std::string& file_id,
                             SyncStatusCode status);
  void DidDetachResourceForCreationConflict(scoped_ptr<SyncTaskToken> token,
                                            google_apis::GDataErrorCode error);

  bool IsContextReady();
  drive::DriveServiceInterface* drive_service();
  drive::DriveUploaderInterface* drive_uploader();
  MetadataDatabase* metadata_database();

  SyncEngineContext* sync_context_;  // Not owned.

  FileChange local_change_;
  bool local_is_missing_;
  base::FilePath local_path_;
  fileapi::FileSystemURL url_;
  SyncAction sync_action_;

  scoped_ptr<FileTracker> remote_file_tracker_;
  scoped_ptr<FileTracker> remote_parent_folder_tracker_;
  base::FilePath target_path_;
  int64 remote_file_change_id_;

  bool retry_on_success_;
  bool needs_remote_change_listing_;

  scoped_ptr<FolderCreator> folder_creator_;

  base::WeakPtrFactory<LocalToRemoteSyncer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalToRemoteSyncer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_

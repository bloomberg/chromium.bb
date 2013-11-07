// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_task.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class ResourceEntry;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;

class RemoteToLocalSyncer : public SyncTask {
 public:
  enum Priority {
    PRIORITY_NORMAL = 1 << 0,
    PRIORITY_LOW = 1 << 1,
  };

  // |priorities| must be a bitwise-or'd value of Priority.
  // Conflicting trackers will have low priority for RemoteToLocalSyncer so that
  // it should be resolved by LocatToRemoteSyncer.
  RemoteToLocalSyncer(SyncEngineContext* sync_context,
                      int priorities);
  virtual ~RemoteToLocalSyncer();
  virtual void Run(const SyncStatusCallback& callback) OVERRIDE;

 private:
  void AnalyzeCurrentDirtyTracker();

  void ResolveRemoteChange(const SyncStatusCallback& callback);

  void GetRemoteResource(const SyncStatusCallback& callback);
  void DidGetRemoteResource(const SyncStatusCallback& callback,
                            int64 change_id,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::ResourceEntry> entry);

  void HandleDeletion(const SyncStatusCallback& callback);
  void DidPrepareForDeletion(const SyncStatusCallback& callback,
                             SyncStatusCode status);

  void HandleNewFile(const SyncStatusCallback& callback);
  void DidPrepareForNewFile(const SyncStatusCallback& callback,
                            SyncStatusCode status);

  void HandleContentUpdate(const SyncStatusCallback& callback);
  void DidPrepareForContentUpdate(const SyncStatusCallback& callback,
                                  SyncStatusCode status);

  void ListFolderContent(const SyncStatusCallback& callback);
  void DidPrepareForFolderListing(const SyncStatusCallback& callback,
                                  SyncStatusCode status);

  void HandleRename(const SyncStatusCallback& callback);
  void HandleReorganize(const SyncStatusCallback& callback);
  void HandleOfflineSolvable(const SyncStatusCallback& callback);

  void SyncCompleted(const SyncStatusCallback& callback);

  void Prepare(const SyncStatusCallback& callback);
  void DidPrepare(const SyncStatusCallback& callback,
                  SyncStatusCode status,
                  const SyncFileMetadata& metadata,
                  const FileChangeList& changes);

  drive::DriveServiceInterface* drive_service();
  MetadataDatabase* metadata_database();
  RemoteChangeProcessor* remote_change_processor();

  SyncEngineContext* sync_context_;  // Not owned.

  int priorities_;
  FileTracker dirty_tracker_;
  FileTracker parent_tracker_;
  FileMetadata remote_metadata_;

  bool missing_remote_details_;
  bool missing_synced_details_;
  bool deleted_remote_details_;
  bool deleted_synced_details_;
  bool title_changed_;
  bool content_changed_;
  bool needs_folder_listing_;
  bool missing_parent_;

  bool sync_root_modification_;

  fileapi::FileSystemURL url_;

  SyncFileMetadata local_metadata_;
  FileChangeList local_changes_;

  base::WeakPtrFactory<RemoteToLocalSyncer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteToLocalSyncer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_

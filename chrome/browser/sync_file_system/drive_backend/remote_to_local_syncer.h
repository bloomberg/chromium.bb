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
  // Dispatches remote change to handlers or to SyncCompleted() directly.
  // This function uses information only in MetadataDatabase.
  //
  // If the tracker doesn't have remote metadata:
  //   # The file is listed in a folder right before this operation.
  //   - Dispatch to HandleMissingRemoteMetadata to fetch remote metadata.
  // Else, if the tracker is not active or the dominating app-root is disabled:
  //   # Assume the file has remote metadata.
  //   - Dispatch to HandleInactiveTracker() to resolve offline solvable
  //     dirtiness.
  // Else, if the tracker doesn't have synced metadata:
  //   # Assume the tracker has remote metadata and the tracker is active.
  //   # The tracker is not yet synced ever.
  //   - If the file is remotely deleted, do nothing to local file and dispatch
  //     directly to SyncCompleted().
  //   - Else, if the file is a regular file, dispatch to HandleNewFile().
  //   - Else, if the file is a folder, dispatch to HandleNewFolder().
  //   - Else, the file should be an unsupported active file. This should not
  //     happen.
  // Else, if the remote metadata is marked as deleted:
  //   # Most of the remote metadata is missing including title, kind and md5.
  //   - Dispatch to HandleDeletion().
  // Else, if the tracker has different titles between its synced metadata and
  // remote metadata:
  //   # Assume the tracker is active and has remote metetadata and synced
  //     metadata.
  //   # The file is remotely renamed.
  //   # Maybe, this can be decomposed to delete and update.
  //   - Dispatch to HandleRemoteRename().
  // Else, if the tracker's parent is not a parent of the remote metadata:
  //   # The file has reorganized.
  //   # Maybe, this can be decomposed to delete and update.
  //   - Dispatch to HandreReorganize().
  // Else, if the folder is a regular file and the md5 in remote metadata does
  // not match the md5 in synced metadata:
  //   # The file is modified remotely.
  //   - Dispatch to HandleContentUpdate().
  // Else, if the tracker is a folder and it has needs_folder_listing flag:
  //   - Dispatch to HandleFolderContentListing()
  // Else, there should be no change to sync.
  //   - Dispatch to HandleOfflineSolvable()
  void ResolveRemoteChange(const SyncStatusCallback& callback);

  // Handles missing remote metadata case.
  // Fetches remote metadata and updates MetadataDatabase by that.  The sync
  // operation itself will be deferred to the next sync round.
  // Note: if the file is not found, it should be handled as if deleted.
  void HandleMissingRemoteMetadata(const SyncStatusCallback& callback);
  void DidGetRemoteMetadata(const SyncStatusCallback& callback,
                            int64 change_id,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::ResourceEntry> entry);
  void DidUpdateDatabaseForRemoteMetadata(const SyncStatusCallback& callback,
                                          SyncStatusCode status);

  // Handles modification to inactive or disabled-app tracker.
  // TODO(tzik): Write details and implement this.
  void HandleInactiveTracker(const SyncStatusCallback& callback);

  // Handles remotely added file.  Needs Prepare() call.
  // TODO(tzik): Write details and implement this.
  void HandleNewFile(const SyncStatusCallback& callback);
  void DidPrepareForNewFile(const SyncStatusCallback& callback,
                            SyncStatusCode status);

  // Handles remotely added folder.  Needs Prepare() call.
  // TODO(tzik): Write details and implement this.
  void HandleNewFolder(const SyncStatusCallback& callback);

  // Handles deleted remote file.  Needs Prepare() call.
  // If the deleted tracker is the sync-root:
  //  - TODO(tzik): Needs special handling.
  // Else, if the deleted tracker is a app-root:
  //  - TODO(tzik): Needs special handling.
  // Else, if the local file is already deleted:
  //  - Do nothing anymore to the local, call SyncCompleted().
  // Else, if the local file is modified:
  //  - Do nothing to the local file, call SyncCompleted().
  // Else, if the local file is not modified:
  //  - Delete local file.
  //  # Note: if the local file is a folder, delete recursively.
  void HandleDeletion(const SyncStatusCallback& callback);
  void DidPrepareForDeletion(const SyncStatusCallback& callback,
                             SyncStatusCode status);

  // TODO(tzik): Write details and implement this.
  void HandleRename(const SyncStatusCallback& callback);

  // TODO(tzik): Write details and implement this.
  void HandleReorganize(const SyncStatusCallback& callback);

  // Handles new file.  Needs Prepare() call.
  void HandleContentUpdate(const SyncStatusCallback& callback);
  void DidPrepareForContentUpdate(const SyncStatusCallback& callback,
                                  SyncStatusCode status);

  void HandleFolderContentListing(const SyncStatusCallback& callback);
  void DidPrepareForFolderListing(const SyncStatusCallback& callback,
                                  SyncStatusCode status);

  void HandleOfflineSolvable(const SyncStatusCallback& callback);

  void SyncCompleted(const SyncStatusCallback& callback);

  void Prepare(const SyncStatusCallback& callback);
  void DidPrepare(const SyncStatusCallback& callback,
                  SyncStatusCode status,
                  const SyncFileMetadata& metadata,
                  const FileChangeList& changes);

  void DeleteLocalFile(const SyncStatusCallback& callback);
  void DidDeleteLocalFile(const SyncStatusCallback& callback,
                          SyncStatusCode status);

  drive::DriveServiceInterface* drive_service();
  MetadataDatabase* metadata_database();
  RemoteChangeProcessor* remote_change_processor();

  SyncEngineContext* sync_context_;  // Not owned.

  int priorities_;
  scoped_ptr<FileTracker> dirty_tracker_;
  scoped_ptr<FileMetadata> remote_metadata_;

  fileapi::FileSystemURL url_;

  SyncFileMetadata local_metadata_;
  FileChangeList local_changes_;

  base::WeakPtrFactory<RemoteToLocalSyncer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteToLocalSyncer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_

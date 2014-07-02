// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "google_apis/drive/gdata_errorcode.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class FileList;
class FileResource;
class ResourceEntry;
}

namespace webkit_blob {
class ScopedFile;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;

class RemoteToLocalSyncer : public SyncTask {
 public:
  // Conflicting trackers will have low priority for RemoteToLocalSyncer so that
  // it should be resolved by LocatToRemoteSyncer.
  explicit RemoteToLocalSyncer(SyncEngineContext* sync_context);
  virtual ~RemoteToLocalSyncer();

  virtual void RunPreflight(scoped_ptr<SyncTaskToken> token) OVERRIDE;
  void RunExclusive(scoped_ptr<SyncTaskToken> token);

  const fileapi::FileSystemURL& url() const { return url_; }
  SyncAction sync_action() const { return sync_action_; }

  bool is_sync_root_deletion() const { return sync_root_deletion_; }

 private:
  typedef std::vector<std::string> FileIDList;

  // TODO(tzik): Update documentation here.
  //
  // Dispatches remote change to handlers or to SyncCompleted() directly.
  // This function uses information only in MetadataDatabase.
  //
  // If the tracker doesn't have remote metadata:
  //   # The file is listed in a folder right before this operation.
  //   - Dispatch to HandleMissingRemoteMetadata to fetch remote metadata.
  // Else, if the tracker is not active or the dominating app-root is disabled:
  //   # Assume the file has remote metadata.
  //   - Update the tracker with |missing| flag and empty |md5|.
  //   Note: MetadataDatabase may activate the tracker if possible.
  // Else, if the tracker doesn't have synced metadata:
  //   # Assume the tracker has remote metadata and the tracker is active.
  //   # The tracker is not yet synced ever.
  //   - If the file is remotely deleted, do nothing to local file and dispatch
  //     directly to SyncCompleted().
  //   - Else, if the file is a regular file, dispatch to HandleNewFile().
  //   - Else, if the file is a folder, dispatch to HandleFolderUpdate().
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
  void ResolveRemoteChange(scoped_ptr<SyncTaskToken> token);

  // Handles missing remote metadata case.
  // Fetches remote metadata and updates MetadataDatabase by that.  The sync
  // operation itself will be deferred to the next sync round.
  // Note: if the file is not found, it should be handled as if deleted.
  void HandleMissingRemoteMetadata(const SyncStatusCallback& callback);
  void DidGetRemoteMetadata(const SyncStatusCallback& callback,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::FileResource> entry);
  void DidUpdateDatabaseForRemoteMetadata(const SyncStatusCallback& callback,
                                          SyncStatusCode status);

  // This implements the body of the HandleNewFile and HandleContentUpdate.
  // If the file doesn't have corresponding local file:
  //   - Dispatch to DownloadFile.
  // Else, if the local file doesn't have local change:
  //   - Dispatch to DownloadFile if the local file is a regular file.
  //   - If the local file is a folder, handle this case as a conflict.  Lower
  //     the priority of the tracker, and defer further handling to
  //     local-to-remote change.
  // Else:
  //  # The file has local modification.
  //  - Handle this case as a conflict.  Lower the priority of the tracker, and
  //    defer further handling to local-to-remote change.
  void DidPrepareForAddOrUpdateFile(const SyncStatusCallback& callback,
                                    SyncStatusCode status);

  // Handles remotely added folder.  Needs Prepare() call.
  // TODO(tzik): Write details and implement this.
  void HandleFolderUpdate(const SyncStatusCallback& callback);
  void DidPrepareForFolderUpdate(const SyncStatusCallback& callback,
                                 SyncStatusCode status);

  void HandleSyncRootDeletion(const SyncStatusCallback& callback);

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

  // Handles new file.  Needs Prepare() call.
  void HandleContentUpdate(const SyncStatusCallback& callback);

  void ListFolderContent(const SyncStatusCallback& callback);
  void DidListFolderContent(
      const SyncStatusCallback& callback,
      scoped_ptr<FileIDList> children,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::FileList> file_list);

  void SyncCompleted(scoped_ptr<SyncTaskToken> token, SyncStatusCode status);
  void FinalizeSync(scoped_ptr<SyncTaskToken> token, SyncStatusCode status);

  void Prepare(const SyncStatusCallback& callback);
  void DidPrepare(const SyncStatusCallback& callback,
                  SyncStatusCode status,
                  const SyncFileMetadata& metadata,
                  const FileChangeList& changes);

  void DeleteLocalFile(const SyncStatusCallback& callback);
  void DownloadFile(const SyncStatusCallback& callback);
  void DidCreateTemporaryFileForDownload(const SyncStatusCallback& callback,
                                         webkit_blob::ScopedFile file);
  void DidDownloadFile(const SyncStatusCallback& callback,
                       webkit_blob::ScopedFile file,
                       google_apis::GDataErrorCode error,
                       const base::FilePath&);
  void DidCalculateMD5ForDownload(const SyncStatusCallback& callback,
                                  webkit_blob::ScopedFile file,
                                  const std::string& md5);
  void DidApplyDownload(const SyncStatusCallback& callback,
                        webkit_blob::ScopedFile,
                        SyncStatusCode status);

  void CreateFolder(const SyncStatusCallback& callback);

  // TODO(tzik): After we convert all callbacks to token-passing style,
  // drop this function.
  SyncStatusCallback SyncCompletedCallback(scoped_ptr<SyncTaskToken> token);

  drive::DriveServiceInterface* drive_service();
  MetadataDatabase* metadata_database();
  RemoteChangeProcessor* remote_change_processor();

  SyncEngineContext* sync_context_;  // Not owned.

  scoped_ptr<FileTracker> dirty_tracker_;
  scoped_ptr<FileMetadata> remote_metadata_;

  fileapi::FileSystemURL url_;
  SyncAction sync_action_;

  bool prepared_;
  bool sync_root_deletion_;

  scoped_ptr<SyncFileMetadata> local_metadata_;
  scoped_ptr<FileChangeList> local_changes_;

  base::WeakPtrFactory<RemoteToLocalSyncer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteToLocalSyncer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_

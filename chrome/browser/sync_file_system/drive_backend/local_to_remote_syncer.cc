// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

scoped_ptr<FileTracker> FindTrackerByID(MetadataDatabase* metadata_database,
                                        int64 tracker_id) {
  scoped_ptr<FileTracker> tracker(new FileTracker);
  if (metadata_database->FindTrackerByTrackerID(tracker_id, tracker.get()))
    return tracker.Pass();
  return scoped_ptr<FileTracker>();
}

void DidUpdateDatabase(const SyncStatusCallback& callback,
                       SyncStatusCode status) {
  if (status == SYNC_STATUS_OK)
    status = SYNC_STATUS_RETRY;
  callback.Run(status);
}

}  // namespace

LocalToRemoteSyncer::LocalToRemoteSyncer(SyncEngineContext* sync_context,
                                         const FileChange& local_change,
                                         const base::FilePath& local_path,
                                         const fileapi::FileSystemURL& url)
    : sync_context_(sync_context),
      local_change_(local_change),
      local_path_(local_path),
      url_(url),
      sync_action_(SYNC_ACTION_NONE),
      weak_ptr_factory_(this) {
}

LocalToRemoteSyncer::~LocalToRemoteSyncer() {
}

void LocalToRemoteSyncer::Run(const SyncStatusCallback& callback) {
  if (!IsContextReady()) {
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  SyncStatusCallback wrapped_callback = base::Bind(
      &LocalToRemoteSyncer::SyncCompleted, weak_ptr_factory_.GetWeakPtr(),
      callback);

  std::string app_id = url_.origin().host();
  base::FilePath path = url_.path();

  scoped_ptr<FileTracker> active_ancestor_tracker(new FileTracker);
  base::FilePath active_ancestor_path;
  if (!metadata_database()->FindNearestActiveAncestor(
          app_id, path,
          active_ancestor_tracker.get(), &active_ancestor_path)) {
    // The app is disabled or not registered.
    callback.Run(SYNC_STATUS_UNKNOWN_ORIGIN);
    return;
  }
  DCHECK(active_ancestor_tracker->active());
  DCHECK(active_ancestor_tracker->has_synced_details());
  const FileDetails& active_ancestor_details =
      active_ancestor_tracker->synced_details();

  // TODO(tzik): Consider handling
  // active_ancestor_tracker->synced_details().missing() case.

  DCHECK(active_ancestor_details.file_kind() == FILE_KIND_FILE ||
         active_ancestor_details.file_kind() == FILE_KIND_FOLDER);

  base::FilePath missing_entries;
  if (active_ancestor_path.empty()) {
    missing_entries = path;
  } else if (active_ancestor_path != path) {
    bool should_success = active_ancestor_path.AppendRelativePath(
        path, &missing_entries);
    if (!should_success) {
      NOTREACHED();
      callback.Run(SYNC_STATUS_FAILED);
      return;
    }
  }

  std::vector<base::FilePath::StringType> missing_components;
  fileapi::VirtualPath::GetComponents(missing_entries, &missing_components);

  if (!missing_components.empty()) {
    if (local_change_.IsDelete() ||
        local_change_.file_type() == SYNC_FILE_TYPE_UNKNOWN) {
      // !IsDelete() case is an error, handle the case as a local deletion case.
      DCHECK(local_change_.IsDelete());

      // Local file is deleted and remote file is missing, already deleted or
      // not yet synced.  There is nothing to do for the file.
      callback.Run(SYNC_STATUS_OK);
      return;
    }
  }

  if (missing_components.size() > 1) {
    // The original target doesn't have remote file and parent.
    // Try creating the parent first.
    if (active_ancestor_details.file_kind() == FILE_KIND_FOLDER) {
      remote_parent_folder_tracker_ = active_ancestor_tracker.Pass();
      target_path_ = active_ancestor_path.Append(missing_components[0]);
      CreateRemoteFolder(wrapped_callback);
      return;
    }

    DCHECK(active_ancestor_details.file_kind() == FILE_KIND_FILE);
    remote_parent_folder_tracker_ =
        FindTrackerByID(metadata_database(),
                        active_ancestor_tracker->parent_tracker_id());
    remote_file_tracker_ = active_ancestor_tracker.Pass();
    target_path_ = active_ancestor_path;
    DeleteRemoteFile(base::Bind(&LocalToRemoteSyncer::DidDeleteForCreateFolder,
                                weak_ptr_factory_.GetWeakPtr(),
                                wrapped_callback));

    return;
  }

  if (missing_components.empty()) {
    // The original target has remote active file/folder.
    remote_parent_folder_tracker_ =
        FindTrackerByID(metadata_database(),
                        active_ancestor_tracker->parent_tracker_id());
    remote_file_tracker_ = active_ancestor_tracker.Pass();
    target_path_ = url_.path();
    DCHECK(target_path_ == active_ancestor_path);

    if (remote_file_tracker_->dirty()) {
      // Both local and remote file has pending modification.
      HandleConflict(wrapped_callback);
      return;
    }

    // Non-conflicting file/folder update case.
    HandleExistingRemoteFile(wrapped_callback);
    return;
  }

  DCHECK(local_change_.IsAddOrUpdate());
  DCHECK_EQ(1u, missing_components.size());
  // The original target has remote parent folder and doesn't have remote active
  // file.
  // Upload the file as a new file or create a folder.
  remote_parent_folder_tracker_ = active_ancestor_tracker.Pass();
  target_path_ = url_.path();
  DCHECK(target_path_ == active_ancestor_path.Append(missing_components[0]));
  if (local_change_.file_type() == SYNC_FILE_TYPE_FILE) {
    UploadNewFile(wrapped_callback);
    return;
  }
  CreateRemoteFolder(wrapped_callback);
}

void LocalToRemoteSyncer::SyncCompleted(const SyncStatusCallback& callback,
                                        SyncStatusCode status) {
  if (status == SYNC_STATUS_OK && target_path_ != url_.path()) {
    callback.Run(SYNC_STATUS_RETRY);
    return;
  }

  callback.Run(status);
}

void LocalToRemoteSyncer::HandleConflict(const SyncStatusCallback& callback) {
  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->has_synced_details());
  DCHECK(remote_file_tracker_->active());
  DCHECK(remote_file_tracker_->dirty());

  if (local_change_.IsFile()) {
    UploadNewFile(callback);
    return;
  }

  DCHECK(local_change_.IsDirectory());
  // Check if we can reuse the remote folder.
  FileMetadata remote_file_metadata;
  bool should_success = metadata_database()->FindFileByFileID(
      remote_file_tracker_->file_id(), &remote_file_metadata);
  if (!should_success) {
    NOTREACHED();
    CreateRemoteFolder(callback);
    return;
  }

  const FileDetails& remote_details = remote_file_metadata.details();
  base::FilePath title = fileapi::VirtualPath::BaseName(target_path_);
  if (!remote_details.missing() &&
      remote_details.file_kind() == FILE_KIND_FOLDER &&
      remote_details.title() == title.AsUTF8Unsafe() &&
      HasFileAsParent(remote_details,
                      remote_parent_folder_tracker_->file_id())) {
    metadata_database()->UpdateTracker(
        remote_file_tracker_->tracker_id(), remote_details, callback);
    return;
  }

  // Create new remote folder.
  CreateRemoteFolder(callback);
}

void LocalToRemoteSyncer::HandleExistingRemoteFile(
    const SyncStatusCallback& callback) {
  DCHECK(remote_file_tracker_);
  DCHECK(!remote_file_tracker_->dirty());
  DCHECK(remote_file_tracker_->active());
  DCHECK(remote_file_tracker_->has_synced_details());

  if (local_change_.IsDelete() ||
      local_change_.file_type() == SYNC_FILE_TYPE_UNKNOWN) {
    // !IsDelete() case is an error, handle the case as a local deletion case.
    DCHECK(local_change_.IsDelete());

    // Local file deletion for existing remote file.
    DeleteRemoteFile(callback);
    return;
  }

  DCHECK(local_change_.IsAddOrUpdate());
  DCHECK(local_change_.file_type() == SYNC_FILE_TYPE_FILE ||
         local_change_.file_type() == SYNC_FILE_TYPE_DIRECTORY);

  const FileDetails& synced_details = remote_file_tracker_->synced_details();
  DCHECK(synced_details.file_kind() == FILE_KIND_FILE ||
         synced_details.file_kind() == FILE_KIND_FOLDER);
  if (local_change_.file_type() == SYNC_FILE_TYPE_FILE) {
    if (synced_details.file_kind() == FILE_KIND_FILE) {
      // Non-conflicting local file update to existing remote regular file.
      UploadExistingFile(callback);
      return;
    }

    DCHECK_EQ(FILE_KIND_FOLDER, synced_details.file_kind());
    // Non-conflicting local file update to existing remote *folder*.
    // Assuming this case as local folder deletion + local file creation, delete
    // the remote folder and upload the file.
    DeleteRemoteFile(base::Bind(&LocalToRemoteSyncer::DidDeleteForUploadNewFile,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback));
    return;
  }

  DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_change_.file_type());
  if (synced_details.file_kind() == FILE_KIND_FILE) {
    // Non-conflicting local folder creation to existing remote *file*.
    // Assuming this case as local file deletion + local folder creation, delete
    // the remote file and create a remote folder.
    DeleteRemoteFile(base::Bind(&LocalToRemoteSyncer::DidDeleteForCreateFolder,
                                weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  // Non-conflicting local folder creation to existing remote folder.
  DCHECK_EQ(FILE_KIND_FOLDER, synced_details.file_kind());
  callback.Run(SYNC_STATUS_OK);
}

void LocalToRemoteSyncer::DeleteRemoteFile(
    const SyncStatusCallback& callback) {
  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->has_synced_details());

  sync_action_ = SYNC_ACTION_DELETED;
  drive_service()->DeleteResource(
      remote_file_tracker_->file_id(),
      remote_file_tracker_->synced_details().etag(),
      base::Bind(&LocalToRemoteSyncer::DidDeleteRemoteFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void LocalToRemoteSyncer::DidDeleteRemoteFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_NOT_FOUND &&
      error != google_apis::HTTP_PRECONDITION &&
      error != google_apis::HTTP_CONFLICT) {
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }

  // Handle NOT_FOUND case as SUCCESS case.
  // For PRECONDITION / CONFLICT case, the remote file is modified since the
  // last sync completed.  As our policy for deletion-modification conflict
  // resolution, ignore the local deletion.
  callback.Run(SYNC_STATUS_OK);
}

void LocalToRemoteSyncer::UploadExistingFile(
    const SyncStatusCallback& callback)  {
  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->has_synced_details());

  base::PostTaskAndReplyWithResult(
      sync_context_->GetBlockingTaskRunner(), FROM_HERE,
      base::Bind(&drive::util::GetMd5Digest, local_path_),
      base::Bind(&LocalToRemoteSyncer::DidGetMD5ForUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void LocalToRemoteSyncer::DidGetMD5ForUpload(
    const SyncStatusCallback& callback,
    const std::string& local_file_md5) {
  if (local_file_md5 == remote_file_tracker_->synced_details().md5()) {
    // Local file is not changed.
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  sync_action_ = SYNC_ACTION_UPDATED;
  drive_uploader()->UploadExistingFile(
      remote_file_tracker_->file_id(),
      local_path_,
      "application/octet_stream",
      remote_file_tracker_->synced_details().etag(),
      base::Bind(&LocalToRemoteSyncer::DidUploadExistingFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      google_apis::ProgressCallback());
}

void LocalToRemoteSyncer::DidUploadExistingFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const GURL&,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error == google_apis::HTTP_PRECONDITION ||
      error == google_apis::HTTP_CONFLICT) {
    // The remote file has unfetched remote change.  Fetch latest metadata and
    // update database with it.
    // TODO(tzik): Consider adding local side low-priority dirtiness handling to
    // handle this as ListChangesTask.
    UpdateRemoteMetadata(callback);
    return;
  }

  metadata_database()->UpdateByFileResource(
      *drive::util::ConvertResourceEntryToFileResource(*entry),
      base::Bind(&LocalToRemoteSyncer::DidUpdateDatabaseForUploadExistingFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void LocalToRemoteSyncer::DidUpdateDatabaseForUploadExistingFile(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  FileMetadata file;
  bool should_success = metadata_database()->FindFileByFileID(
      remote_file_tracker_->file_id(), &file);
  if (!should_success) {
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  const FileDetails& details = file.details();
  base::FilePath title = fileapi::VirtualPath::BaseName(target_path_);
  if (!details.missing() &&
      details.file_kind() == FILE_KIND_FILE &&
      details.title() == title.AsUTF8Unsafe() &&
      HasFileAsParent(details,
                      remote_parent_folder_tracker_->file_id())) {
    metadata_database()->UpdateTracker(
        remote_file_tracker_->tracker_id(),
        file.details(),
        callback);
    return;
  }

  callback.Run(SYNC_STATUS_RETRY);
}

void LocalToRemoteSyncer::UpdateRemoteMetadata(
    const SyncStatusCallback& callback) {
  DCHECK(remote_file_tracker_);
  drive_service()->GetResourceEntry(
      remote_file_tracker_->file_id(),
      base::Bind(&LocalToRemoteSyncer::DidGetRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void LocalToRemoteSyncer::DidGetRemoteMetadata(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  metadata_database()->UpdateByFileResource(
      *drive::util::ConvertResourceEntryToFileResource(*entry),
      base::Bind(&DidUpdateDatabase, callback));
}

void LocalToRemoteSyncer::DidDeleteForUploadNewFile(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status == SYNC_STATUS_HAS_CONFLICT) {
    UpdateRemoteMetadata(callback);
    return;
  }

  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  UploadNewFile(callback);
}

void LocalToRemoteSyncer::DidDeleteForCreateFolder(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status == SYNC_STATUS_HAS_CONFLICT) {
    UpdateRemoteMetadata(callback);
    return;
  }

  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  CreateRemoteFolder(callback);
}

void LocalToRemoteSyncer::UploadNewFile(const SyncStatusCallback& callback) {
  DCHECK(remote_parent_folder_tracker_);

  sync_action_ = SYNC_ACTION_ADDED;
  base::FilePath title = fileapi::VirtualPath::BaseName(target_path_);
  drive_uploader()->UploadNewFile(
      remote_parent_folder_tracker_->file_id(),
      local_path_,
      title.AsUTF8Unsafe(),
      GetMimeTypeFromTitle(title),
      base::Bind(&LocalToRemoteSyncer::DidUploadNewFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      google_apis::ProgressCallback());
}

void LocalToRemoteSyncer::DidUploadNewFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const GURL& upload_location,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }

  metadata_database()->ReplaceActiveTrackerWithNewResource(
      remote_parent_folder_tracker_->tracker_id(),
      *drive::util::ConvertResourceEntryToFileResource(*entry),
      callback);
}

void LocalToRemoteSyncer::CreateRemoteFolder(
    const SyncStatusCallback& callback) {
  base::FilePath title = fileapi::VirtualPath::BaseName(target_path_);
  DCHECK(remote_parent_folder_tracker_);
  sync_action_ = SYNC_ACTION_ADDED;
  drive_service()->AddNewDirectory(
      remote_parent_folder_tracker_->file_id(),
      title.AsUTF8Unsafe(),
      base::Bind(&LocalToRemoteSyncer::DidCreateRemoteFolder,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void LocalToRemoteSyncer::DidCreateRemoteFolder(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }

  drive_service()->SearchByTitle(
      entry->title(), remote_parent_folder_tracker_->file_id(),
      base::Bind(&LocalToRemoteSyncer::DidListFolderForEnsureUniqueness,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Passed(ScopedVector<google_apis::ResourceEntry>())));
}

void LocalToRemoteSyncer::DidListFolderForEnsureUniqueness(
    const SyncStatusCallback& callback,
    ScopedVector<google_apis::ResourceEntry> candidates,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }

  candidates.reserve(candidates.size() + resource_list->entries().size());
  candidates.insert(candidates.end(),
                    resource_list->entries().begin(),
                    resource_list->entries().end());
  resource_list->mutable_entries()->weak_clear();

  GURL next_feed;
  if (resource_list->GetNextFeedURL(&next_feed)) {
    drive_service()->GetRemainingFileList(
        next_feed,
        base::Bind(&LocalToRemoteSyncer::DidListFolderForEnsureUniqueness,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   base::Passed(&candidates)));
    return;
  }

  ScopedVector<google_apis::FileResource> files;
  files.reserve(candidates.size());
  for (size_t i = 0; i < candidates.size(); ++i) {
    files.push_back(drive::util::ConvertResourceEntryToFileResource(
        *candidates[i]).release());
  }

  scoped_ptr<google_apis::ResourceEntry> oldest =
      GetOldestCreatedFolderResource(candidates.Pass());
  std::string file_id = oldest->resource_id();

  metadata_database()->UpdateByFileResourceList(
      files.Pass(),
      base::Bind(&LocalToRemoteSyncer::DidUpdateDatabaseForCreateNewFolder,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, file_id));
}

void LocalToRemoteSyncer::DidUpdateDatabaseForCreateNewFolder(
    const SyncStatusCallback& callback,
    const std::string& file_id,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  if (metadata_database()->TryNoSideEffectActivation(
          remote_parent_folder_tracker_->tracker_id(),
          file_id, callback)) {
    // |callback| will be invoked by MetadataDatabase in this case.
    return;
  }

  drive_service()->RemoveResourceFromDirectory(
      remote_parent_folder_tracker_->file_id(), file_id,
      base::Bind(&LocalToRemoteSyncer::DidDetachResourceForCreationConflict,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void LocalToRemoteSyncer::DidDetachResourceForCreationConflict(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }

  callback.Run(SYNC_STATUS_RETRY);
}

bool LocalToRemoteSyncer::IsContextReady() {
  return sync_context_->GetDriveService() &&
      sync_context_->GetDriveUploader() &&
      sync_context_->GetMetadataDatabase();
}

drive::DriveServiceInterface* LocalToRemoteSyncer::drive_service() {
  set_used_network(true);
  return sync_context_->GetDriveService();
}

drive::DriveUploaderInterface* LocalToRemoteSyncer::drive_uploader() {
  set_used_network(true);
  return sync_context_->GetDriveUploader();
}

MetadataDatabase* LocalToRemoteSyncer::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

}  // namespace drive_backend
}  // namespace sync_file_system

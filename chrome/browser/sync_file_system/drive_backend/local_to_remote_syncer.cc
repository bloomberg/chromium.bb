// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"

#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

scoped_ptr<FileTracker> FindTracker(MetadataDatabase* metadata_database,
                                    const std::string& app_id,
                                    const base::FilePath& full_path) {
  DCHECK(metadata_database);

  base::FilePath path;
  scoped_ptr<FileTracker> tracker(new FileTracker);
  if (metadata_database->FindNearestActiveAncestor(
          app_id, path, tracker.get(), &path) &&
      path == full_path) {
    DCHECK(tracker->active());
    return tracker.Pass();
  }
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
                                         const SyncFileMetadata& local_metadata,
                                         const fileapi::FileSystemURL& url)
    : sync_context_(sync_context),
      local_change_(local_change),
      local_path_(local_path),
      local_metadata_(local_metadata),
      weak_ptr_factory_(this) {
}

LocalToRemoteSyncer::~LocalToRemoteSyncer() {
}

void LocalToRemoteSyncer::Run(const SyncStatusCallback& callback) {
  if (!drive_service() || !drive_uploader() || !metadata_database()) {
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  remote_file_tracker_ = FindTracker(metadata_database(),
                                     url_.origin().host(), url_.path());
  DCHECK(!remote_file_tracker_ || remote_file_tracker_->active());

  if (!remote_file_tracker_ ||
      !remote_file_tracker_->has_synced_details() ||
      remote_file_tracker_->synced_details().missing()) {
    // Remote file is missing, deleted or not yet synced.
    HandleMissingRemoteFile(callback);
    return;
  }

  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->active());
  DCHECK(remote_file_tracker_->has_synced_details());
  DCHECK(!remote_file_tracker_->synced_details().missing());

  // An active tracker is found at the path.
  // Check if the local change conflicts a remote change, and resolve it if
  // needed.
  if (remote_file_tracker_->dirty()) {
    HandleConflict(callback);
    return;
  }

  DCHECK(!remote_file_tracker_->dirty());
  HandleExistingRemoteFile(callback);
}

void LocalToRemoteSyncer::HandleMissingRemoteFile(
    const SyncStatusCallback& callback) {
  if (local_change_.IsDelete() ||
      local_change_.file_type() == SYNC_FILE_TYPE_UNKNOWN) {
    // !IsDelete() case is an error, handle the case as a local deletion case.
    DCHECK(local_change_.IsDelete());

    // Local file is deleted and remote file is missing, already deleted or not
    // yet synced.  There is nothing to do for the file.
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  DCHECK(local_change_.IsAddOrUpdate());
  DCHECK(local_change_.file_type() == SYNC_FILE_TYPE_FILE ||
         local_change_.file_type() == SYNC_FILE_TYPE_DIRECTORY);

  if (PopulateRemoteParentFolder()) {
    DCHECK(!remote_parent_folder_tracker_);

    // Missing remote parent folder.
    // TODO(tzik): Create remote parent folder and finish current sync phase as
    // SYNC_STATUS_RETRY.
    NOTIMPLEMENTED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  DCHECK(remote_parent_folder_tracker_);
  if (local_change_.file_type() == SYNC_FILE_TYPE_FILE) {
    // Upload local file as a new file.
    UploadNewFile(callback);
    return;
  }

  DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_change_.file_type());
  // Create remote folder.
  UploadNewFile(callback);
}

void LocalToRemoteSyncer::HandleConflict(const SyncStatusCallback& callback) {
  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->dirty());

  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
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
      error != google_apis::HTTP_PRECONDITION) {
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }

  // Handle NOT_FOUND case as SUCCESS case.
  // For PRECONDITION case, the remote file is modified since the last sync
  // completed.  As our policy for deletion-modification conflict resolution,
  // ignore the local deletion.
  callback.Run(SYNC_STATUS_OK);
}

bool LocalToRemoteSyncer::PopulateRemoteParentFolder() {
  NOTIMPLEMENTED();
  return false;
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
    scoped_ptr<google_apis::ResourceEntry>) {
  if (error == google_apis::HTTP_PRECONDITION) {
    // The remote file has unfetched remote change.  Fetch latest metadata and
    // update database with it.
    // TODO(tzik): Consider adding local side low-priority dirtiness handling to
    // handle this as ListChangesTask.
    UpdateRemoteMetadata(callback);
    return;
  }

  callback.Run(GDataErrorCodeToSyncStatusCode(error));
}

void LocalToRemoteSyncer::UpdateRemoteMetadata(
    const SyncStatusCallback& callback) {
  DCHECK(remote_file_tracker_);
  drive_service()->GetResourceEntry(
      remote_file_tracker_->file_id(),
      base::Bind(&LocalToRemoteSyncer::DidGetRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 metadata_database()->GetLargestKnownChangeID()));
}

void LocalToRemoteSyncer::DidGetRemoteMetadata(
    const SyncStatusCallback& callback,
    int64 change_id,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  metadata_database()->UpdateByFileResource(
      change_id,
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
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

void LocalToRemoteSyncer::CreateRemoteFolder(
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

drive::DriveServiceInterface* LocalToRemoteSyncer::drive_service() {
  return sync_context_->GetDriveService();
}

drive::DriveUploaderInterface* LocalToRemoteSyncer::drive_uploader() {
  return sync_context_->GetDriveUploader();
}

MetadataDatabase* LocalToRemoteSyncer::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

}  // namespace drive_backend
}  // namespace sync_file_system

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"

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
  NOTIMPLEMENTED();
}

void LocalToRemoteSyncer::Run(const SyncStatusCallback& callback) {
  if (!drive_service() || !drive_uploader() || !metadata_database()) {
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  remote_file_tracker_ = FindTracker(metadata_database(),
                                     url_.origin().host(), url_.path());
  if (remote_file_tracker_) {
    // An active tracker is found at the path.
    // Check if the local change conflicts a remote change, and resolve it if
    // needed.
    if (remote_file_tracker_->dirty()) {
      HandleConflict(callback);
      return;
    }

    DCHECK(!remote_file_tracker_->dirty());
    HandleExistingRemoteFile(callback);
    return;
  }

  // No active tracker is found at the path.
  HandleMissingRemoteFile(callback);
}

void LocalToRemoteSyncer::HandleMissingRemoteFile(
    const SyncStatusCallback& callback) {
  DCHECK(!remote_file_tracker_);

  if (local_change_.IsDelete() ||
      local_change_.file_type() == SYNC_FILE_TYPE_UNKNOWN) {
    // !IsDelete() case is an error, handle the case as a local deletion case.
    DCHECK(local_change_.IsDelete());

    // Local file is deleted and remote file is missing.  There is nothing to do
    // for the file.
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
    NOTIMPLEMENTED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_change_.file_type());
  // Create remote folder.
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
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

  if (local_change_.IsDelete() ||
      local_change_.file_type() == SYNC_FILE_TYPE_UNKNOWN) {
    // !IsDelete() case is an error, handle the case as a local deletion case.
    DCHECK(local_change_.IsDelete());

    // Local file deletion for existing remote file.
    // TODO(tzik): Check remote modification, delete if there's no remote
    // modification.
    NOTIMPLEMENTED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  DCHECK(local_change_.IsAddOrUpdate());
  DCHECK(local_change_.file_type() == SYNC_FILE_TYPE_FILE ||
         local_change_.file_type() == SYNC_FILE_TYPE_DIRECTORY);

  if (local_change_.file_type() == SYNC_FILE_TYPE_FILE) {
    NOTIMPLEMENTED();
    callback.Run(SYNC_STATUS_FAILED);
  }

  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

bool LocalToRemoteSyncer::PopulateRemoteParentFolder() {
  NOTIMPLEMENTED();
  return false;
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

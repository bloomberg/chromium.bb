// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/local_sync_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/sync_file_system/conflict_resolution_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/api_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

LocalSyncDelegate::LocalSyncDelegate(
    DriveFileSyncService* sync_service,
    const FileChange& local_change,
    const base::FilePath& local_path,
    const SyncFileMetadata& local_metadata,
    const fileapi::FileSystemURL& url)
    : sync_service_(sync_service),
      operation_(SYNC_OPERATION_NONE),
      url_(url),
      local_change_(local_change),
      local_path_(local_path),
      local_metadata_(local_metadata),
      has_drive_metadata_(false),
      has_remote_change_(false),
      weak_factory_(this) {}

LocalSyncDelegate::~LocalSyncDelegate() {}

void LocalSyncDelegate::Run(const SyncStatusCallback& callback) {
  DCHECK(!local_change_.IsDirectory());
  operation_ = SYNC_OPERATION_NONE;

  has_drive_metadata_ =
      metadata_store()->ReadEntry(url_, &drive_metadata_) == SYNC_STATUS_OK;

  if (!has_drive_metadata_)
    drive_metadata_.set_md5_checksum(std::string());

  sync_service_->EnsureOriginRootDirectory(
      url_.origin(),
      base::Bind(&LocalSyncDelegate::DidGetOriginRoot,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void LocalSyncDelegate::DidGetOriginRoot(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    const std::string& origin_resource_id) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  origin_resource_id_ = origin_resource_id;

  has_remote_change_ =
      remote_change_handler()->GetChangeForURL(url_, &remote_change_);
  if (has_remote_change_ && drive_metadata_.resource_id().empty())
    drive_metadata_.set_resource_id(remote_change_.resource_id);

  SyncFileType remote_file_type =
      has_remote_change_ ? remote_change_.change.file_type() :
      has_drive_metadata_ ?
          DriveFileSyncService::DriveMetadataResourceTypeToSyncFileType(
              drive_metadata_.type())
      : SYNC_FILE_TYPE_UNKNOWN;

  DCHECK_EQ(SYNC_OPERATION_NONE, operation_);
  operation_ = LocalSyncOperationResolver::Resolve(
      local_change_,
      has_remote_change_ ? &remote_change_.change : NULL,
      has_drive_metadata_ ? &drive_metadata_ : NULL);

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "ApplyLocalChange for %s local_change:%s ===> %s",
            url_.DebugString().c_str(),
            local_change_.DebugString().c_str(),
            SyncOperationTypeToString(operation_));

  switch (operation_) {
    case SYNC_OPERATION_ADD_FILE:
      UploadNewFile(callback);
      return;
    case SYNC_OPERATION_ADD_DIRECTORY:
      CreateDirectory(callback);
      return;
    case SYNC_OPERATION_UPDATE_FILE:
      UploadExistingFile(callback);
      return;
    case SYNC_OPERATION_DELETE:
      Delete(callback);
      return;
    case SYNC_OPERATION_NONE:
      callback.Run(SYNC_STATUS_OK);
      return;
    case SYNC_OPERATION_CONFLICT:
      HandleConflict(callback);
      return;
    case SYNC_OPERATION_RESOLVE_TO_LOCAL:
      ResolveToLocal(callback);
      return;
    case SYNC_OPERATION_RESOLVE_TO_REMOTE:
      ResolveToRemote(callback, remote_file_type);
      return;
    case SYNC_OPERATION_DELETE_METADATA:
      DeleteMetadata(base::Bind(
          &LocalSyncDelegate::DidApplyLocalChange,
          weak_factory_.GetWeakPtr(), callback, google_apis::HTTP_SUCCESS));
      return;
    case SYNC_OPERATION_FAIL: {
      callback.Run(SYNC_STATUS_FAILED);
      return;
    }
  }
  NOTREACHED();
  callback.Run(SYNC_STATUS_FAILED);
}

void LocalSyncDelegate::UploadNewFile(const SyncStatusCallback& callback) {
  api_util()->UploadNewFile(
      origin_resource_id_,
      local_path_,
      DriveFileSyncService::PathToTitle(url_.path()),
      base::Bind(&LocalSyncDelegate::DidUploadNewFile,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::DidUploadNewFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& md5) {
  switch (error) {
    case google_apis::HTTP_CREATED:
      UpdateMetadata(
          resource_id, md5, DriveMetadata::RESOURCE_TYPE_FILE,
          base::Bind(&LocalSyncDelegate::DidApplyLocalChange,
                     weak_factory_.GetWeakPtr(), callback, error));
      sync_service_->NotifyObserversFileStatusChanged(
          url_,
          SYNC_FILE_STATUS_SYNCED,
          SYNC_ACTION_ADDED,
          SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    case google_apis::HTTP_CONFLICT:
      HandleCreationConflict(resource_id, DriveMetadata::RESOURCE_TYPE_FILE,
                             callback);
      return;
    default:
      callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
  }
}

void LocalSyncDelegate::CreateDirectory(const SyncStatusCallback& callback) {
  NOTREACHED();
  api_util()->CreateDirectory(
      origin_resource_id_,
      DriveFileSyncService::PathToTitle(url_.path()),
      base::Bind(&LocalSyncDelegate::DidCreateDirectory,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::DidCreateDirectory(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  switch (error) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED: {
      UpdateMetadata(
          resource_id, std::string(), DriveMetadata::RESOURCE_TYPE_FOLDER,
          base::Bind(&LocalSyncDelegate::DidApplyLocalChange,
                     weak_factory_.GetWeakPtr(), callback, error));
      sync_service_->NotifyObserversFileStatusChanged(
          url_,
          SYNC_FILE_STATUS_SYNCED,
          SYNC_ACTION_ADDED,
          SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    }

    case google_apis::HTTP_CONFLICT:
      // There were conflicts and a file was left.
      // TODO(kinuko): Handle the latter case (http://crbug.com/237090).
      // Fall-through

    default:
      callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
  }
}

void LocalSyncDelegate::UploadExistingFile(const SyncStatusCallback& callback) {
  DCHECK(has_drive_metadata_);
  if (drive_metadata_.resource_id().empty()) {
    UploadNewFile(callback);
    return;
  }

  api_util()->UploadExistingFile(
      drive_metadata_.resource_id(),
      drive_metadata_.md5_checksum(),
      local_path_,
      base::Bind(&LocalSyncDelegate::DidUploadExistingFile,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::DidUploadExistingFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& md5) {
  DCHECK(has_drive_metadata_);
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      UpdateMetadata(
          resource_id, md5, DriveMetadata::RESOURCE_TYPE_FILE,
          base::Bind(&LocalSyncDelegate::DidApplyLocalChange,
                     weak_factory_.GetWeakPtr(), callback, error));
      sync_service_->NotifyObserversFileStatusChanged(
          url_,
          SYNC_FILE_STATUS_SYNCED,
          SYNC_ACTION_UPDATED,
          SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    case google_apis::HTTP_CONFLICT:
      HandleConflict(callback);
      return;
    case google_apis::HTTP_NOT_MODIFIED:
      DidApplyLocalChange(callback,
                          google_apis::HTTP_SUCCESS, SYNC_STATUS_OK);
      return;
    case google_apis::HTTP_NOT_FOUND:
      UploadNewFile(callback);
      return;
    default: {
      const SyncStatusCode status =
          GDataErrorCodeToSyncStatusCodeWrapper(error);
      DCHECK_NE(SYNC_STATUS_OK, status);
      callback.Run(status);
      return;
    }
  }
}

void LocalSyncDelegate::Delete(const SyncStatusCallback& callback) {
  if (!has_drive_metadata_) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  if (drive_metadata_.resource_id().empty()) {
    DidDelete(callback, google_apis::HTTP_NOT_FOUND);
    return;
  }

  api_util()->DeleteFile(
      drive_metadata_.resource_id(),
      drive_metadata_.md5_checksum(),
      base::Bind(&LocalSyncDelegate::DidDelete,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::DidDelete(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(has_drive_metadata_);

  switch (error) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_NOT_FOUND:
      DeleteMetadata(base::Bind(
          &LocalSyncDelegate::DidApplyLocalChange,
          weak_factory_.GetWeakPtr(), callback, google_apis::HTTP_SUCCESS));
      sync_service_->NotifyObserversFileStatusChanged(
          url_,
          SYNC_FILE_STATUS_SYNCED,
          SYNC_ACTION_DELETED,
          SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    case google_apis::HTTP_PRECONDITION:
    case google_apis::HTTP_CONFLICT:
      // Delete |drive_metadata| on the conflict case.
      // Conflicted remote change should be applied as a future remote change.
      DeleteMetadata(base::Bind(
          &LocalSyncDelegate::DidDeleteMetadataForDeletionConflict,
          weak_factory_.GetWeakPtr(), callback));
      sync_service_->NotifyObserversFileStatusChanged(
          url_,
          SYNC_FILE_STATUS_SYNCED,
          SYNC_ACTION_DELETED,
          SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    default: {
      const SyncStatusCode status =
          GDataErrorCodeToSyncStatusCodeWrapper(error);
      DCHECK_NE(SYNC_STATUS_OK, status);
      callback.Run(status);
      return;
    }
  }
}

void LocalSyncDelegate::DidDeleteMetadataForDeletionConflict(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  callback.Run(SYNC_STATUS_OK);
}

void LocalSyncDelegate::ResolveToLocal(const SyncStatusCallback& callback) {
  if (drive_metadata_.resource_id().empty()) {
    DidDeleteFileToResolveToLocal(callback, google_apis::HTTP_NOT_FOUND);
    return;
  }

  api_util()->DeleteFile(
      drive_metadata_.resource_id(),
      drive_metadata_.md5_checksum(),
      base::Bind(
          &LocalSyncDelegate::DidDeleteFileToResolveToLocal,
          weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::DidDeleteFileToResolveToLocal(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_NOT_FOUND) {
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
    return;
  }

  DCHECK_NE(SYNC_FILE_TYPE_UNKNOWN, local_metadata_.file_type);
  if (local_metadata_.file_type == SYNC_FILE_TYPE_FILE) {
    UploadNewFile(callback);
    return;
  }

  NOTREACHED();
  DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_metadata_.file_type);
  CreateDirectory(callback);
}

void LocalSyncDelegate::ResolveToRemote(
    const SyncStatusCallback& callback,
    SyncFileType remote_file_type) {
  // Mark the file as to-be-fetched.
  DCHECK(!drive_metadata_.resource_id().empty());

  SetMetadataToBeFetched(
      DriveFileSyncService::SyncFileTypeToDriveMetadataResourceType(
          remote_file_type),
      base::Bind(&LocalSyncDelegate::DidResolveToRemote,
                 weak_factory_.GetWeakPtr(), callback));
  // The synced notification will be dispatched when the remote file is
  // downloaded.
}

void LocalSyncDelegate::DidResolveToRemote(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  DCHECK(has_drive_metadata_);
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  SyncFileType file_type = SYNC_FILE_TYPE_FILE;
  if (drive_metadata_.type() == DriveMetadata::RESOURCE_TYPE_FOLDER)
    file_type = SYNC_FILE_TYPE_DIRECTORY;
  sync_service_->AppendFetchChange(
      url_.origin(), url_.path(), drive_metadata_.resource_id(), file_type);
  callback.Run(status);
}

void LocalSyncDelegate::DidApplyLocalChange(
    const SyncStatusCallback& callback,
    const google_apis::GDataErrorCode error,
    SyncStatusCode status) {
  if ((operation_ == SYNC_OPERATION_DELETE ||
       operation_ == SYNC_OPERATION_DELETE_METADATA) &&
      (status == SYNC_FILE_ERROR_NOT_FOUND ||
       status == SYNC_DATABASE_ERROR_NOT_FOUND)) {
    status = SYNC_STATUS_OK;
  }

  if (status == SYNC_STATUS_OK) {
    remote_change_handler()->RemoveChangeForURL(url_);
    status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  }
  callback.Run(status);
}

void LocalSyncDelegate::UpdateMetadata(
    const std::string& resource_id,
    const std::string& md5,
    DriveMetadata::ResourceType type,
    const SyncStatusCallback& callback) {
  has_drive_metadata_ = true;
  drive_metadata_.set_resource_id(resource_id);
  drive_metadata_.set_md5_checksum(md5);
  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(false);
  drive_metadata_.set_type(type);
  metadata_store()->UpdateEntry(url_, drive_metadata_, callback);
}

void LocalSyncDelegate::ResetMetadataForStartOver(
    const SyncStatusCallback& callback) {
  has_drive_metadata_ = true;
  DCHECK(!drive_metadata_.resource_id().empty());
  drive_metadata_.set_md5_checksum(std::string());
  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(false);
  metadata_store()->UpdateEntry(url_, drive_metadata_, callback);
}

void LocalSyncDelegate::SetMetadataToBeFetched(
    DriveMetadata::ResourceType type,
    const SyncStatusCallback& callback) {
  has_drive_metadata_ = true;
  drive_metadata_.set_md5_checksum(std::string());
  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(true);
  drive_metadata_.set_type(type);
  metadata_store()->UpdateEntry(url_, drive_metadata_, callback);
}

void LocalSyncDelegate::DeleteMetadata(const SyncStatusCallback& callback) {
  metadata_store()->DeleteEntry(url_, callback);
}

void LocalSyncDelegate::HandleCreationConflict(
    const std::string& resource_id,
    DriveMetadata::ResourceType type,
    const SyncStatusCallback& callback) {
  // File-file conflict is found.
  // Populates a fake drive_metadata and set has_drive_metadata = true.
  // In HandleConflictLocalSync:
  // - If conflict_resolution is manual, we'll change conflicted to true
  //   and save the metadata.
  // - Otherwise we'll save the metadata with empty md5 and will start
  //   over local sync as UploadExistingFile.
  drive_metadata_.set_resource_id(resource_id);
  drive_metadata_.set_md5_checksum(std::string());
  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(false);
  drive_metadata_.set_type(type);
  has_drive_metadata_ = true;
  HandleConflict(callback);
}

void LocalSyncDelegate::HandleConflict(const SyncStatusCallback& callback) {
  DCHECK(!drive_metadata_.resource_id().empty());
  api_util()->GetResourceEntry(
      drive_metadata_.resource_id(),
      base::Bind(
          &LocalSyncDelegate::DidGetEntryForConflictResolution,
          weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::DidGetEntryForConflictResolution(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  SyncFileType remote_file_type = SYNC_FILE_TYPE_UNKNOWN;
  ConflictResolution resolution = CONFLICT_RESOLUTION_UNKNOWN;

  if (error != google_apis::HTTP_SUCCESS ||
      entry->updated_time().is_null()) {
    resolution = CONFLICT_RESOLUTION_LOCAL_WIN;
  } else {
    SyncFileType local_file_type = local_metadata_.file_type;
    base::Time local_modification_time = local_metadata_.last_modified;
    base::Time remote_modification_time = entry->updated_time();
    if (entry->is_file())
      remote_file_type = SYNC_FILE_TYPE_FILE;
    else if (entry->is_folder())
      remote_file_type = SYNC_FILE_TYPE_DIRECTORY;
    else
      remote_file_type = SYNC_FILE_TYPE_UNKNOWN;

    resolution = conflict_resolution_resolver()->Resolve(
        local_file_type, local_modification_time,
        remote_file_type, remote_modification_time);
  }

  switch (resolution) {
    case CONFLICT_RESOLUTION_MARK_CONFLICT:
      HandleManualResolutionCase(callback);
      return;
    case CONFLICT_RESOLUTION_LOCAL_WIN:
      HandleLocalWinCase(callback);
      return;
    case CONFLICT_RESOLUTION_REMOTE_WIN:
      HandleRemoteWinCase(callback, remote_file_type);
      return;
    case CONFLICT_RESOLUTION_UNKNOWN:
      NOTREACHED();
  }
  NOTREACHED();
  callback.Run(SYNC_STATUS_FAILED);
}

void LocalSyncDelegate::HandleManualResolutionCase(
    const SyncStatusCallback& callback) {
  if (drive_metadata_.conflicted()) {
    callback.Run(SYNC_STATUS_HAS_CONFLICT);
    return;
  }

  has_drive_metadata_ = true;
  sync_service_->MarkConflict(
      url_, &drive_metadata_,
      base::Bind(&LocalSyncDelegate::DidMarkConflict,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::DidMarkConflict(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  DidApplyLocalChange(callback, google_apis::HTTP_CONFLICT, status);
}

void LocalSyncDelegate::HandleLocalWinCase(
    const SyncStatusCallback& callback) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Resolving conflict for local sync: %s: LOCAL WIN",
            url_.DebugString().c_str());

  DCHECK(!drive_metadata_.resource_id().empty());
  if (!has_drive_metadata_) {
    StartOver(callback, SYNC_STATUS_OK);
    return;
  }

  ResetMetadataForStartOver(base::Bind(&LocalSyncDelegate::StartOver,
                                       weak_factory_.GetWeakPtr(), callback));
}

void LocalSyncDelegate::HandleRemoteWinCase(
    const SyncStatusCallback& callback,
    SyncFileType remote_file_type) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Resolving conflict for local sync: %s: REMOTE WIN",
            url_.DebugString().c_str());
  ResolveToRemote(callback, remote_file_type);
}

void LocalSyncDelegate::StartOver(const SyncStatusCallback& callback,
                                  SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  remote_change_handler()->RemoveChangeForURL(url_);

  // Return the control back to the sync service once.
  callback.Run(SYNC_STATUS_RETRY);
}

SyncStatusCode
LocalSyncDelegate::GDataErrorCodeToSyncStatusCodeWrapper(
    google_apis::GDataErrorCode error) {
  return sync_service_->GDataErrorCodeToSyncStatusCodeWrapper(error);
}

DriveMetadataStore* LocalSyncDelegate::metadata_store() {
  return sync_service_->metadata_store_.get();
}

APIUtilInterface* LocalSyncDelegate::api_util() {
  return sync_service_->api_util_.get();
}

RemoteChangeHandler* LocalSyncDelegate::remote_change_handler() {
  return &sync_service_->remote_change_handler_;
}

ConflictResolutionResolver* LocalSyncDelegate::conflict_resolution_resolver() {
  return &sync_service_->conflict_resolution_resolver_;
}

}  // namespace drive_backend
}  // namespace sync_file_system

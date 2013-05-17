// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive/local_change_processor_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/sync_file_system/drive/api_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace sync_file_system {
namespace drive {

LocalChangeProcessorDelegate::LocalChangeProcessorDelegate(
    base::WeakPtr<DriveFileSyncService> sync_service,
    const FileChange& local_change,
    const base::FilePath& local_path,
    const SyncFileMetadata& local_metadata,
    const fileapi::FileSystemURL& url)
    : sync_service_(sync_service),
      url_(url),
      local_change_(local_change),
      local_path_(local_path),
      local_metadata_(local_metadata),
      has_drive_metadata_(false),
      has_remote_change_(false),
      weak_factory_(this) {}

LocalChangeProcessorDelegate::~LocalChangeProcessorDelegate() {}

void LocalChangeProcessorDelegate::Run(const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  // TODO(nhiroki): support directory operations (http://crbug.com/161442).
  DCHECK(IsSyncDirectoryOperationEnabled() || !local_change_.IsDirectory());

  has_drive_metadata_ =
      metadata_store()->ReadEntry(url_, &drive_metadata_) == SYNC_STATUS_OK;

  if (!has_drive_metadata_)
    drive_metadata_.set_md5_checksum(std::string());

  sync_service_->EnsureOriginRootDirectory(
      url_.origin(),
      base::Bind(&LocalChangeProcessorDelegate::DidGetOriginRoot,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void LocalChangeProcessorDelegate::DidGetOriginRoot(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    const std::string& origin_resource_id) {
  if (!sync_service_)
    return;

  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  origin_resource_id_ = origin_resource_id;

  has_remote_change_ =
      remote_change_handler()->GetChangeForURL(url_, &remote_change_);
  if (has_remote_change_ && drive_metadata_.resource_id().empty())
    drive_metadata_.set_resource_id(remote_change_.resource_id);

  LocalSyncOperationType operation = LocalSyncOperationResolver::Resolve(
      local_change_,
      has_remote_change_ ? &remote_change_.change : NULL,
      has_drive_metadata_ ? &drive_metadata_ : NULL);

  DVLOG(1) << "ApplyLocalChange for " << url_.DebugString()
           << " local_change:" << local_change_.DebugString()
           << " ==> operation:" << operation;

  switch (operation) {
    case LOCAL_SYNC_OPERATION_ADD_FILE:
      UploadNewFile(callback);
      return;
    case LOCAL_SYNC_OPERATION_ADD_DIRECTORY:
      CreateDirectory(callback);
      return;
    case LOCAL_SYNC_OPERATION_UPDATE_FILE:
      UploadExistingFile(callback);
      return;
    case LOCAL_SYNC_OPERATION_DELETE_FILE:
      DeleteFile(callback);
      return;
    case LOCAL_SYNC_OPERATION_DELETE_DIRECTORY:
      DeleteDirectory(callback);
      return;
    case LOCAL_SYNC_OPERATION_NONE:
      callback.Run(SYNC_STATUS_OK);
      return;
    case LOCAL_SYNC_OPERATION_CONFLICT:
      HandleConflict(callback);
      return;
    case LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL:
      ResolveToLocal(callback);
      return;
    case LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE:
      ResolveToRemote(callback);
      return;
    case LOCAL_SYNC_OPERATION_DELETE_METADATA:
      DeleteMetadata(base::Bind(
          &LocalChangeProcessorDelegate::DidApplyLocalChange,
          weak_factory_.GetWeakPtr(), callback, google_apis::HTTP_SUCCESS));
      return;
    case LOCAL_SYNC_OPERATION_FAIL: {
      callback.Run(SYNC_STATUS_FAILED);
      return;
    }
  }
  NOTREACHED();
  callback.Run(SYNC_STATUS_FAILED);
}

void LocalChangeProcessorDelegate::UploadNewFile(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  api_util()->UploadNewFile(
      origin_resource_id_,
      local_path_,
      DriveFileSyncService::PathToTitle(url_.path()),
      base::Bind(&LocalChangeProcessorDelegate::DidUploadNewFile,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::DidUploadNewFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& md5) {
  if (!sync_service_)
    return;

  switch (error) {
    case google_apis::HTTP_CREATED:
      UpdateMetadata(
          resource_id, md5, DriveMetadata::RESOURCE_TYPE_FILE,
          base::Bind(&LocalChangeProcessorDelegate::DidApplyLocalChange,
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

void LocalChangeProcessorDelegate::CreateDirectory(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  DCHECK(IsSyncDirectoryOperationEnabled());
  api_util()->CreateDirectory(
      origin_resource_id_,
      DriveFileSyncService::PathToTitle(url_.path()),
      base::Bind(&LocalChangeProcessorDelegate::DidCreateDirectory,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::DidCreateDirectory(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  if (!sync_service_)
    return;

  switch (error) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED: {
      UpdateMetadata(
          resource_id, std::string(), DriveMetadata::RESOURCE_TYPE_FOLDER,
          base::Bind(&LocalChangeProcessorDelegate::DidApplyLocalChange,
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

void LocalChangeProcessorDelegate::UploadExistingFile(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  DCHECK(has_drive_metadata_);
  api_util()->UploadExistingFile(
      drive_metadata_.resource_id(),
      drive_metadata_.md5_checksum(),
      local_path_,
      base::Bind(&LocalChangeProcessorDelegate::DidUploadExistingFile,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::DidUploadExistingFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& md5) {
  if (!sync_service_)
    return;

  DCHECK(has_drive_metadata_);
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      UpdateMetadata(
          resource_id, md5, DriveMetadata::RESOURCE_TYPE_FILE,
          base::Bind(&LocalChangeProcessorDelegate::DidApplyLocalChange,
                     weak_factory_.GetWeakPtr(), callback, error));
      sync_service_->NotifyObserversFileStatusChanged(
          url_,
          SYNC_FILE_STATUS_SYNCED,
          SYNC_ACTION_UPDATED,
          SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    case google_apis::HTTP_CONFLICT: {
      HandleConflict(callback);
      return;
    }
    case google_apis::HTTP_NOT_MODIFIED: {
      DidApplyLocalChange(callback,
                          google_apis::HTTP_SUCCESS, SYNC_STATUS_OK);
      return;
    }
    case google_apis::HTTP_NOT_FOUND: {
      UploadNewFile(callback);
      return;
    }
    default: {
      const SyncStatusCode status =
          GDataErrorCodeToSyncStatusCodeWrapper(error);
      DCHECK_NE(SYNC_STATUS_OK, status);
      callback.Run(status);
      return;
    }
  }
}

void LocalChangeProcessorDelegate::DeleteFile(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  DCHECK(has_drive_metadata_);
  api_util()->DeleteFile(
      drive_metadata_.resource_id(),
      drive_metadata_.md5_checksum(),
      base::Bind(&LocalChangeProcessorDelegate::DidDeleteFile,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::DeleteDirectory(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  DCHECK(IsSyncDirectoryOperationEnabled());
  DCHECK(has_drive_metadata_);
  // This does not handle recursive directory deletion
  // (which should not happen other than after a restart).
  api_util()->DeleteFile(
      drive_metadata_.resource_id(),
      std::string(),  // empty md5
      base::Bind(&LocalChangeProcessorDelegate::DidDeleteFile,
                 weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::DidDeleteFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  if (!sync_service_)
    return;

  DCHECK(has_drive_metadata_);

  switch (error) {
    // Regardless of whether the deletion has succeeded (HTTP_SUCCESS) or
    // has failed with ETag conflict error (HTTP_PRECONDITION or HTTP_CONFLICT)
    // we should just delete the drive_metadata.
    // In the former case the file should be just gone now, and
    // in the latter case the remote change will be applied in a future
    // remote sync.
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_PRECONDITION:
    case google_apis::HTTP_CONFLICT:
      DeleteMetadata(base::Bind(
          &LocalChangeProcessorDelegate::DidApplyLocalChange,
          weak_factory_.GetWeakPtr(), callback, error));
      sync_service_->NotifyObserversFileStatusChanged(
          url_,
          SYNC_FILE_STATUS_SYNCED,
          SYNC_ACTION_DELETED,
          SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    case google_apis::HTTP_NOT_FOUND:
      DidApplyLocalChange(callback,
                          google_apis::HTTP_SUCCESS, SYNC_STATUS_OK);
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

void LocalChangeProcessorDelegate::ResolveToLocal(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  api_util()->DeleteFile(
      drive_metadata_.resource_id(),
      drive_metadata_.md5_checksum(),
      base::Bind(
          &LocalChangeProcessorDelegate::DidDeleteFileToResolveToLocal,
          weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::DidDeleteFileToResolveToLocal(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  if (!sync_service_)
    return;

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_NOT_FOUND) {
    remote_change_handler()->RemoveChangeForURL(url_);
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
    return;
  }

  DCHECK_NE(SYNC_FILE_TYPE_UNKNOWN, local_metadata_.file_type);
  if (local_metadata_.file_type == SYNC_FILE_TYPE_FILE) {
    UploadNewFile(callback);
    return;
  }

  DCHECK(IsSyncDirectoryOperationEnabled());
  DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_metadata_.file_type);
  CreateDirectory(callback);
}

void LocalChangeProcessorDelegate::ResolveToRemote(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  // Mark the file as to-be-fetched.
  DCHECK(!drive_metadata_.resource_id().empty());

  SyncFileType type = remote_change_.change.file_type();
  SetMetadataToBeFetched(
      DriveFileSyncService::SyncFileTypeToDriveMetadataResourceType(type),
      base::Bind(&LocalChangeProcessorDelegate::DidResolveToRemote,
                 weak_factory_.GetWeakPtr(), callback));
  // The synced notification will be dispatched when the remote file is
  // downloaded.
}

void LocalChangeProcessorDelegate::DidResolveToRemote(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (!sync_service_)
    return;

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

void LocalChangeProcessorDelegate::DidApplyLocalChange(
    const SyncStatusCallback& callback,
    const google_apis::GDataErrorCode error,
    SyncStatusCode status) {
  if (!sync_service_)
    return;

  if (status == SYNC_STATUS_OK) {
    remote_change_handler()->RemoveChangeForURL(url_);
    status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  }
  callback.Run(status);
}

void LocalChangeProcessorDelegate::UpdateMetadata(
    const std::string& resource_id,
    const std::string& md5,
    DriveMetadata::ResourceType type,
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  drive_metadata_.set_resource_id(resource_id);
  drive_metadata_.set_md5_checksum(md5);
  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(false);
  drive_metadata_.set_type(type);
  metadata_store()->UpdateEntry(url_, drive_metadata_, callback);
}

void LocalChangeProcessorDelegate::ResetMetadataMD5(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  drive_metadata_.set_md5_checksum(std::string());
  metadata_store()->UpdateEntry(url_, drive_metadata_, callback);
}

void LocalChangeProcessorDelegate::SetMetadataToBeFetched(
    DriveMetadata::ResourceType type,
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  drive_metadata_.set_md5_checksum(std::string());
  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(true);
  drive_metadata_.set_type(type);
  metadata_store()->UpdateEntry(url_, drive_metadata_, callback);
}

void LocalChangeProcessorDelegate::SetMetadataConflict(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  drive_metadata_.set_conflicted(true);
  drive_metadata_.set_to_be_fetched(false);
  metadata_store()->UpdateEntry(url_, drive_metadata_, callback);
}

void LocalChangeProcessorDelegate::DeleteMetadata(
    const SyncStatusCallback& callback) {
  metadata_store()->DeleteEntry(url_, callback);
}

void LocalChangeProcessorDelegate::HandleCreationConflict(
    const std::string& resource_id,
    DriveMetadata::ResourceType type,
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

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

void LocalChangeProcessorDelegate::HandleConflict(
    const SyncStatusCallback& callback) {
  if (!sync_service_)
    return;

  DCHECK(!drive_metadata_.resource_id().empty());
  api_util()->GetResourceEntry(
      drive_metadata_.resource_id(),
      base::Bind(
          &LocalChangeProcessorDelegate::DidGetEntryForConflictResolution,
          weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::DidGetEntryForConflictResolution(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (!sync_service_)
    return;

  SyncFileType local_file_type = local_metadata_.file_type;
  base::Time local_modification_time = local_metadata_.last_modified;

  SyncFileType remote_file_type;
  base::Time remote_modification_time = entry->updated_time();
  if (entry->is_file())
    remote_file_type = SYNC_FILE_TYPE_FILE;
  else if (entry->is_folder())
    remote_file_type = SYNC_FILE_TYPE_DIRECTORY;
  else
    remote_file_type = SYNC_FILE_TYPE_UNKNOWN;

  DriveFileSyncService::ConflictResolutionResult resolution =
      sync_service_->ResolveConflictForLocalSync(
          local_file_type, local_modification_time,
          remote_file_type, remote_modification_time);
  switch (resolution) {
    case DriveFileSyncService::CONFLICT_RESOLUTION_MARK_CONFLICT:
      HandleManualResolutionCase(callback);
      return;
    case DriveFileSyncService::CONFLICT_RESOLUTION_LOCAL_WIN:
      HandleLocalWinCase(callback);
      return;
    case DriveFileSyncService::CONFLICT_RESOLUTION_REMOTE_WIN:
      HandleRemoteWinCase(callback);
      return;
  }
  NOTREACHED();
  callback.Run(SYNC_STATUS_FAILED);
}

void LocalChangeProcessorDelegate::HandleManualResolutionCase(
    const SyncStatusCallback& callback) {
  if (drive_metadata_.conflicted()) {
    callback.Run(SYNC_STATUS_HAS_CONFLICT);
    return;
  }

  SetMetadataConflict(
      base::Bind(&LocalChangeProcessorDelegate::DidApplyLocalChange,
                 weak_factory_.GetWeakPtr(), callback,
                 google_apis::HTTP_CONFLICT));
}

void LocalChangeProcessorDelegate::HandleLocalWinCase(
    const SyncStatusCallback& callback) {
  DVLOG(1) << "Resolving conflict for local sync:"
           << url_.DebugString() << ": LOCAL WIN";

  DCHECK(!drive_metadata_.resource_id().empty());
  if (!has_drive_metadata_) {
    StartOver(callback, SYNC_STATUS_OK);
    return;
  }

  ResetMetadataMD5(base::Bind(&LocalChangeProcessorDelegate::StartOver,
                              weak_factory_.GetWeakPtr(), callback));
}

void LocalChangeProcessorDelegate::HandleRemoteWinCase(
    const SyncStatusCallback& callback) {
  DVLOG(1) << "Resolving conflict for local sync:"
           << url_.DebugString() << ": REMOTE WIN";
  ResolveToRemote(callback);
}

void LocalChangeProcessorDelegate::StartOver(const SyncStatusCallback& callback,
                                             SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  remote_change_handler()->RemoveChangeForURL(url_);
  Run(callback);
}

SyncStatusCode
LocalChangeProcessorDelegate::GDataErrorCodeToSyncStatusCodeWrapper(
    google_apis::GDataErrorCode error) {
  return sync_service_->GDataErrorCodeToSyncStatusCodeWrapper(error);
}

DriveMetadataStore* LocalChangeProcessorDelegate::metadata_store() {
  return sync_service_->metadata_store_.get();
}

APIUtilInterface* LocalChangeProcessorDelegate::api_util() {
  return sync_service_->api_util_.get();
}

RemoteChangeHandler* LocalChangeProcessorDelegate::remote_change_handler() {
  return &sync_service_->remote_change_handler_;
}

}  // namespace drive
}  // namespace sync_file_system

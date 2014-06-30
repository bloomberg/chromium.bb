// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/remote_sync_delegate.h"

#include "base/file_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/remote_sync_operation_resolver.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"

using fileapi::FileSystemURL;

namespace {

void EmptyStatusCallback(sync_file_system::SyncStatusCode status) {}

}  // namespace

namespace sync_file_system {
namespace drive_backend {

RemoteSyncDelegate::RemoteSyncDelegate(
    DriveFileSyncService* sync_service,
    const RemoteChange& remote_change)
    : sync_service_(sync_service),
      remote_change_(remote_change),
      sync_action_(SYNC_ACTION_NONE),
      metadata_updated_(false),
      clear_local_changes_(true) {
}

RemoteSyncDelegate::~RemoteSyncDelegate() {}

void RemoteSyncDelegate::Run(const SyncStatusCallback& callback) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "ProcessRemoteChange for %s change:%s",
            url().DebugString().c_str(),
            remote_file_change().DebugString().c_str());

  remote_change_processor()->PrepareForProcessRemoteChange(
      url(),
      base::Bind(&RemoteSyncDelegate::DidPrepareForProcessRemoteChange,
                 AsWeakPtr(), callback));
}

void RemoteSyncDelegate::DidPrepareForProcessRemoteChange(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    const SyncFileMetadata& metadata,
    const FileChangeList& local_changes) {
  if (status != SYNC_STATUS_OK) {
    AbortSync(callback, status);
    return;
  }

  local_metadata_ = metadata;
  status = metadata_store()->ReadEntry(url(), &drive_metadata_);
  DCHECK(status == SYNC_STATUS_OK || status == SYNC_DATABASE_ERROR_NOT_FOUND);

  bool missing_db_entry = (status != SYNC_STATUS_OK);
  if (missing_db_entry) {
    drive_metadata_.set_resource_id(remote_change_.resource_id);
    drive_metadata_.set_md5_checksum(std::string());
    drive_metadata_.set_conflicted(false);
    drive_metadata_.set_to_be_fetched(false);
  }
  bool missing_local_file = (metadata.file_type == SYNC_FILE_TYPE_UNKNOWN);

  if (drive_metadata_.resource_id().empty()) {
    // This (missing_db_entry is false but resource_id is empty) could
    // happen when the remote file gets deleted (this clears resource_id
    // in drive_metadata) but then a file is added with the same name.
    drive_metadata_.set_resource_id(remote_change_.resource_id);
  }

  SyncOperationType operation =
      RemoteSyncOperationResolver::Resolve(remote_file_change(),
                                           local_changes,
                                           local_metadata_.file_type,
                                           drive_metadata_.conflicted());

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "ProcessRemoteChange for %s %s%sremote_change: %s ==> %s",
            url().DebugString().c_str(),
            drive_metadata_.conflicted() ? " (conflicted)" : " ",
            missing_local_file ? " (missing local file)" : " ",
            remote_file_change().DebugString().c_str(),
            SyncOperationTypeToString(operation));
  DCHECK_NE(SYNC_OPERATION_FAIL, operation);

  switch (operation) {
    case SYNC_OPERATION_ADD_FILE:
    case SYNC_OPERATION_ADD_DIRECTORY:
      sync_action_ = SYNC_ACTION_ADDED;
      break;
    case SYNC_OPERATION_UPDATE_FILE:
      sync_action_ = SYNC_ACTION_UPDATED;
      break;
    case SYNC_OPERATION_DELETE:
      sync_action_ = SYNC_ACTION_DELETED;
      break;
    case SYNC_OPERATION_NONE:
    case SYNC_OPERATION_DELETE_METADATA:
      sync_action_ = SYNC_ACTION_NONE;
      break;
    default:
      break;
  }

  switch (operation) {
    case SYNC_OPERATION_ADD_FILE:
    case SYNC_OPERATION_UPDATE_FILE:
      DownloadFile(callback);
      return;
    case SYNC_OPERATION_ADD_DIRECTORY:
    case SYNC_OPERATION_DELETE:
      ApplyRemoteChange(callback);
      return;
    case SYNC_OPERATION_NONE:
      CompleteSync(callback, SYNC_STATUS_OK);
      return;
    case SYNC_OPERATION_CONFLICT:
      HandleConflict(callback, remote_file_change().file_type());
      return;
    case SYNC_OPERATION_RESOLVE_TO_LOCAL:
      ResolveToLocal(callback);
      return;
    case SYNC_OPERATION_RESOLVE_TO_REMOTE:
      ResolveToRemote(callback);
      return;
    case SYNC_OPERATION_DELETE_METADATA:
      if (missing_db_entry)
        CompleteSync(callback, SYNC_STATUS_OK);
      else
        DeleteMetadata(callback);
      return;
    case SYNC_OPERATION_FAIL:
      AbortSync(callback, SYNC_STATUS_FAILED);
      return;
  }
  NOTREACHED();
  AbortSync(callback, SYNC_STATUS_FAILED);
}

void RemoteSyncDelegate::ApplyRemoteChange(const SyncStatusCallback& callback) {
  remote_change_processor()->ApplyRemoteChange(
      remote_file_change(), temporary_file_.path(), url(),
      base::Bind(&RemoteSyncDelegate::DidApplyRemoteChange, AsWeakPtr(),
                 callback));
}

void RemoteSyncDelegate::DidApplyRemoteChange(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    AbortSync(callback, status);
    return;
  }

  if (remote_file_change().IsDelete()) {
    DeleteMetadata(callback);
    return;
  }

  drive_metadata_.set_resource_id(remote_change_.resource_id);
  drive_metadata_.set_conflicted(false);
  if (remote_file_change().IsFile()) {
    drive_metadata_.set_type(DriveMetadata::RESOURCE_TYPE_FILE);
  } else {
    NOTREACHED();
    drive_metadata_.set_type(DriveMetadata::RESOURCE_TYPE_FOLDER);
  }

  metadata_store()->UpdateEntry(
      url(), drive_metadata_,
      base::Bind(&RemoteSyncDelegate::CompleteSync,
                 AsWeakPtr(), callback));
}

void RemoteSyncDelegate::DeleteMetadata(const SyncStatusCallback& callback) {
  metadata_store()->DeleteEntry(
      url(),
      base::Bind(&RemoteSyncDelegate::CompleteSync, AsWeakPtr(), callback));
}

void RemoteSyncDelegate::DownloadFile(const SyncStatusCallback& callback) {
  // We should not use the md5 in metadata for FETCH type to avoid the download
  // finishes due to NOT_MODIFIED.
  std::string md5_checksum;
  if (!drive_metadata_.to_be_fetched())
    md5_checksum = drive_metadata_.md5_checksum();

  api_util()->DownloadFile(
      remote_change_.resource_id,
      md5_checksum,
      base::Bind(&RemoteSyncDelegate::DidDownloadFile,
                 AsWeakPtr(),
                 callback));
}

void RemoteSyncDelegate::DidDownloadFile(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& md5_checksum,
    int64 file_size,
    const base::Time& updated_time,
    webkit_blob::ScopedFile downloaded_file) {
  if (error == google_apis::HTTP_NOT_MODIFIED) {
    sync_action_ = SYNC_ACTION_NONE;
    DidApplyRemoteChange(callback, SYNC_STATUS_OK);
    return;
  }

  // File may be deleted. If this was for new file it's ok, if this was
  // for existing file we'll process the delete change later.
  if (error == google_apis::HTTP_NOT_FOUND) {
    sync_action_ = SYNC_ACTION_NONE;
    DidApplyRemoteChange(callback, SYNC_STATUS_OK);
    return;
  }

  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status != SYNC_STATUS_OK) {
    AbortSync(callback, status);
    return;
  }

  temporary_file_ = downloaded_file.Pass();
  drive_metadata_.set_md5_checksum(md5_checksum);
  remote_change_processor()->ApplyRemoteChange(
      remote_file_change(), temporary_file_.path(), url(),
      base::Bind(&RemoteSyncDelegate::DidApplyRemoteChange,
                 AsWeakPtr(), callback));
}

void RemoteSyncDelegate::HandleConflict(
    const SyncStatusCallback& callback,
    SyncFileType remote_file_type) {
  ConflictResolution resolution = conflict_resolution_resolver()->Resolve(
      local_metadata_.file_type,
      local_metadata_.last_modified,
      remote_file_type,
      remote_change_.updated_time);

  switch (resolution) {
    case CONFLICT_RESOLUTION_LOCAL_WIN:
      HandleLocalWin(callback);
      return;
    case CONFLICT_RESOLUTION_REMOTE_WIN:
      HandleRemoteWin(callback, remote_file_type);
      return;
    case CONFLICT_RESOLUTION_MARK_CONFLICT:
      HandleManualResolutionCase(callback);
      return;
    case CONFLICT_RESOLUTION_UNKNOWN:
      // Get remote file time and call this method again.
      api_util()->GetResourceEntry(
          remote_change_.resource_id,
          base::Bind(
              &RemoteSyncDelegate::DidGetEntryForConflictResolution,
              AsWeakPtr(), callback));
      return;
  }
  NOTREACHED();
  AbortSync(callback, SYNC_STATUS_FAILED);
}

void RemoteSyncDelegate::HandleLocalWin(
    const SyncStatusCallback& callback) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Resolving conflict for remote sync: %s: LOCAL WIN",
            url().DebugString().c_str());
  ResolveToLocal(callback);
}

void RemoteSyncDelegate::HandleRemoteWin(
    const SyncStatusCallback& callback,
    SyncFileType remote_file_type) {
  // Make sure we reset the conflict flag and start over the remote sync
  // with empty local changes.
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Resolving conflict for remote sync: %s: REMOTE WIN",
            url().DebugString().c_str());

  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(false);
  drive_metadata_.set_type(
      DriveFileSyncService::SyncFileTypeToDriveMetadataResourceType(
          remote_file_type));
  metadata_store()->UpdateEntry(
      url(), drive_metadata_,
      base::Bind(&RemoteSyncDelegate::StartOver, AsWeakPtr(), callback));
}

void RemoteSyncDelegate::HandleManualResolutionCase(
    const SyncStatusCallback& callback) {
  sync_action_ = SYNC_ACTION_NONE;
  sync_service_->MarkConflict(
      url(), &drive_metadata_,
      base::Bind(&RemoteSyncDelegate::CompleteSync, AsWeakPtr(), callback));
}

void RemoteSyncDelegate::DidGetEntryForConflictResolution(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status != SYNC_STATUS_OK || entry->updated_time().is_null()) {
    HandleLocalWin(callback);
    return;
  }

  SyncFileType file_type = SYNC_FILE_TYPE_UNKNOWN;
  if (entry->is_file())
    file_type = SYNC_FILE_TYPE_FILE;
  if (entry->is_folder())
    file_type = SYNC_FILE_TYPE_DIRECTORY;

  remote_change_.updated_time = entry->updated_time();
  HandleConflict(callback, file_type);
}

void RemoteSyncDelegate::ResolveToLocal(
    const SyncStatusCallback& callback) {
  sync_action_ = SYNC_ACTION_NONE;
  clear_local_changes_ = false;

  // Re-add a fake local change to resolve it later in next LocalSync.
  remote_change_processor()->RecordFakeLocalChange(
      url(),
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 local_metadata_.file_type),
      base::Bind(&RemoteSyncDelegate::DidResolveToLocal,
                 AsWeakPtr(), callback));
}

void RemoteSyncDelegate::DidResolveToLocal(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    DCHECK_NE(SYNC_STATUS_HAS_CONFLICT, status);
    AbortSync(callback, status);
    return;
  }

  if (remote_file_change().IsDelete()) {
    metadata_store()->DeleteEntry(
        url(),
        base::Bind(&RemoteSyncDelegate::CompleteSync,
                   AsWeakPtr(), callback));
  } else {
    DCHECK(!remote_change_.resource_id.empty());
    drive_metadata_.set_resource_id(remote_change_.resource_id);
    drive_metadata_.set_conflicted(false);
    drive_metadata_.set_to_be_fetched(false);
    drive_metadata_.set_md5_checksum(std::string());
    metadata_store()->UpdateEntry(
        url(), drive_metadata_,
        base::Bind(&RemoteSyncDelegate::CompleteSync,
                   AsWeakPtr(), callback));
  }
}

void RemoteSyncDelegate::ResolveToRemote(
    const SyncStatusCallback& callback) {
  drive_metadata_.set_conflicted(false);
  drive_metadata_.set_to_be_fetched(true);
  metadata_store()->UpdateEntry(
      url(), drive_metadata_,
      base::Bind(&RemoteSyncDelegate::DidResolveToRemote,
                 AsWeakPtr(), callback));
}

void RemoteSyncDelegate::DidResolveToRemote(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    AbortSync(callback, status);
    return;
  }

  sync_action_ = SYNC_ACTION_ADDED;
  if (remote_file_change().file_type() == SYNC_FILE_TYPE_FILE) {
    DownloadFile(callback);
    return;
  }

  // ApplyRemoteChange should replace any existing local file or
  // directory with remote_change_.
  ApplyRemoteChange(callback);
}

void RemoteSyncDelegate::StartOver(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  DidPrepareForProcessRemoteChange(
      callback, status, local_metadata_, FileChangeList());
}

void RemoteSyncDelegate::CompleteSync(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    AbortSync(callback, status);
    return;
  }

  sync_service_->RemoveRemoteChange(url());

  if (drive_metadata_.to_be_fetched()) {
    // Clear |to_be_fetched| flag since we completed fetching the remote change
    // and applying it to the local file.
    DCHECK(!drive_metadata_.conflicted());
    drive_metadata_.set_conflicted(false);
    drive_metadata_.set_to_be_fetched(false);
    metadata_store()->UpdateEntry(url(), drive_metadata_,
                                  base::Bind(&EmptyStatusCallback));
  }

  if (remote_change_.changestamp > 0) {
    DCHECK(metadata_store()->IsIncrementalSyncOrigin(url().origin()));
    metadata_store()->SetLargestChangeStamp(
        remote_change_.changestamp,
        base::Bind(&RemoteSyncDelegate::DidFinish, AsWeakPtr(), callback));
    return;
  }

  if (drive_metadata_.conflicted())
    status = SYNC_STATUS_HAS_CONFLICT;

  DidFinish(callback, status);
}

void RemoteSyncDelegate::AbortSync(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  clear_local_changes_ = false;
  DidFinish(callback, status);
}

void RemoteSyncDelegate::DidFinish(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  remote_change_processor()->FinalizeRemoteSync(
      url(), clear_local_changes_,
      base::Bind(&RemoteSyncDelegate::DispatchCallbackAfterDidFinish,
                 AsWeakPtr(), callback, status));
}

void RemoteSyncDelegate::DispatchCallbackAfterDidFinish(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status == SYNC_STATUS_OK && sync_action_ != SYNC_ACTION_NONE) {
    sync_service_->NotifyObserversFileStatusChanged(
        url(),
        SYNC_FILE_STATUS_SYNCED,
        sync_action_,
        SYNC_DIRECTION_REMOTE_TO_LOCAL);
  }

  callback.Run(status);
}

SyncStatusCode RemoteSyncDelegate::GDataErrorCodeToSyncStatusCodeWrapper(
    google_apis::GDataErrorCode error) {
  return sync_service_->GDataErrorCodeToSyncStatusCodeWrapper(error);
}

DriveMetadataStore* RemoteSyncDelegate::metadata_store() {
  return sync_service_->metadata_store_.get();
}

APIUtilInterface* RemoteSyncDelegate::api_util() {
  return sync_service_->api_util_.get();
}

RemoteChangeHandler* RemoteSyncDelegate::remote_change_handler() {
  return &sync_service_->remote_change_handler_;
}

RemoteChangeProcessor* RemoteSyncDelegate::remote_change_processor() {
  return sync_service_->remote_change_processor_;
}

ConflictResolutionResolver* RemoteSyncDelegate::conflict_resolution_resolver() {
  return &sync_service_->conflict_resolution_resolver_;
}

}  // namespace drive_backend
}  // namespace sync_file_system

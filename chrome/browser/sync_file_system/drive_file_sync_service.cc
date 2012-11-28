// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_file_type.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace sync_file_system {

const char DriveFileSyncService::kServiceName[] = "drive";

class DriveFileSyncService::TaskToken {
 public:
  explicit TaskToken(const base::WeakPtr<DriveFileSyncService>& sync_service)
      : sync_service_(sync_service),
        task_type_(TASK_TYPE_NONE) {
  }

  void ResetTask(const tracked_objects::Location& location) {
    location_ = location;
    task_type_ = TASK_TYPE_NONE;
    description_.clear();
  }

  void UpdateTask(const tracked_objects::Location& location,
                  TaskType task_type,
                  const std::string& description) {
    location_ = location;
    task_type_ = task_type;
    description_ = description;
  }

  const tracked_objects::Location& location() const { return location_; }
  TaskType task_type() const { return task_type_; }
  const std::string& description() const { return description_; }

  ~TaskToken() {
    // All task on DriveFileSyncService must hold TaskToken instance to ensure
    // no other tasks are running. Also, as soon as a task finishes to work,
    // it must return the token to DriveFileSyncService.
    // Destroying a token with valid |sync_service_| indicates the token was
    // dropped by a task without returning.
    DCHECK(!sync_service_);
    if (sync_service_) {
      LOG(ERROR) << "Unexpected TaskToken deletion from: "
                 << location_.ToString() << " while: " << description_;
    }
  }

 private:
  base::WeakPtr<DriveFileSyncService> sync_service_;
  tracked_objects::Location location_;
  TaskType task_type_;
  std::string description_;

  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

DriveFileSyncService::ChangeQueueItem::ChangeQueueItem() {
}

DriveFileSyncService::ChangeQueueItem::ChangeQueueItem(
    int64 changestamp,
    const fileapi::FileSystemURL& url)
    : changestamp(changestamp),
      url(url) {
}

bool DriveFileSyncService::ChangeQueueComparator::operator()(
    const ChangeQueueItem& left,
    const ChangeQueueItem& right) {
  if (left.changestamp != right.changestamp)
    return left.changestamp < right.changestamp;
  return fileapi::FileSystemURL::Comparator()(left.url, right.url);
}

DriveFileSyncService::RemoteChange::RemoteChange()
    : changestamp(0),
      change(fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE,
             fileapi::SYNC_FILE_TYPE_UNKNOWN) {
}

DriveFileSyncService::RemoteChange::RemoteChange(
    int64 changestamp,
    const std::string& resource_id,
    const fileapi::FileSystemURL& url,
    const fileapi::FileChange& change,
    PendingChangeQueue::iterator position_in_queue)
    : changestamp(changestamp),
      url(url),
      change(change),
      position_in_queue(position_in_queue) {
}

DriveFileSyncService::RemoteChange::~RemoteChange() {
}

bool DriveFileSyncService::RemoteChangeComparator::operator()(
    const RemoteChange& left,
    const RemoteChange& right) {
  // This should return true if |right| has higher priority than |left|.
  // Smaller changestamps have higher priorities (i.e. need to be processed
  // earlier).
  if (left.changestamp != right.changestamp)
    return left.changestamp > right.changestamp;
  return false;
}

DriveFileSyncService::DriveFileSyncService(Profile* profile)
    : last_operation_status_(fileapi::SYNC_STATUS_OK),
      state_(REMOTE_SERVICE_OK),
      largest_changestamp_(0),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  token_.reset(new TaskToken(AsWeakPtr()));

  sync_client_.reset(new DriveFileSyncClient(profile));

  metadata_store_.reset(new DriveMetadataStore(
      profile->GetPath(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE)));

  metadata_store_->Initialize(
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore, AsWeakPtr(),
                 base::Passed(GetToken(FROM_HERE, TASK_TYPE_DATABASE,
                                       "Metadata database initialization"))));
}

DriveFileSyncService::~DriveFileSyncService() {
  // Invalidate WeakPtr instances here explicitly to notify TaskToken that we
  // can safely discard the token.
  weak_factory_.InvalidateWeakPtrs();
  token_.reset();
}

// static
scoped_ptr<DriveFileSyncService> DriveFileSyncService::CreateForTesting(
    scoped_ptr<DriveFileSyncClient> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store) {
  return make_scoped_ptr(new DriveFileSyncService(
      sync_client.Pass(), metadata_store.Pass()));
}

void DriveFileSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DriveFileSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DriveFileSyncService::RegisterOriginForTrackingChanges(
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(
          FROM_HERE, TASK_TYPE_DRIVE, "Retrieving origin metadata"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::RegisterOriginForTrackingChanges,
        AsWeakPtr(), origin, callback));
    return;
  }

  if (state_ == REMOTE_SERVICE_DISABLED) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(last_operation_status_, token.Pass());
    callback.Run(last_operation_status_);
    return;
  }

  if (metadata_store_->IsIncrementalSyncOrigin(origin) ||
      metadata_store_->IsBatchSyncOrigin(origin)) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
    callback.Run(fileapi::SYNC_STATUS_OK);
    return;
  }

  DCHECK(!metadata_store_->sync_root_directory().empty());
  sync_client_->GetDriveDirectoryForOrigin(
      metadata_store_->sync_root_directory(), origin,
      base::Bind(&DriveFileSyncService::DidGetDirectoryForOrigin,
                 AsWeakPtr(), base::Passed(&token), origin, callback));
}

void DriveFileSyncService::UnregisterOriginForTrackingChanges(
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE, TASK_TYPE_DATABASE, ""));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::UnregisterOriginForTrackingChanges,
        AsWeakPtr(), origin, callback));
    return;
  }

  URLToChange::iterator found = url_to_change_.find(origin);
  if (found != url_to_change_.end()) {
    for (PathToChange::iterator itr = found->second.begin();
         itr != found->second.end(); ++itr)
      pending_changes_.erase(itr->second.position_in_queue);
    url_to_change_.erase(found);
  }

  metadata_store_->RemoveOrigin(origin, base::Bind(
      &DriveFileSyncService::DidRemoveOriginOnMetadataStore,
      AsWeakPtr(), base::Passed(&token), callback));
}

void DriveFileSyncService::ProcessRemoteChange(
    RemoteChangeProcessor* processor,
    const fileapi::SyncOperationCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(fileapi::SYNC_STATUS_FAILED,
               fileapi::FileSystemURL(),
               fileapi::SYNC_OPERATION_NONE);
}

LocalChangeProcessor* DriveFileSyncService::GetLocalChangeProcessor() {
  return this;
}

void DriveFileSyncService::GetConflictFiles(
    const GURL& origin,
    const fileapi::SyncFileSetCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(fileapi::SYNC_STATUS_FAILED,
               fileapi::FileSystemURLSet());
}

void DriveFileSyncService::GetRemoteFileMetadata(
    const fileapi::FileSystemURL& url,
    const fileapi::SyncFileMetadataCallback& callback) {
  DriveMetadata metadata;
  const fileapi::SyncStatusCode status =
      metadata_store_->ReadEntry(url, &metadata);
  if (status != fileapi::SYNC_STATUS_OK)
    callback.Run(status, fileapi::SyncFileMetadata());
  sync_client_->GetDocumentEntry(
      metadata.resource_id(),
      base::Bind(&DriveFileSyncService::DidGetRemoteFileMetadata,
                 AsWeakPtr(), callback));
}

RemoteServiceState DriveFileSyncService::GetCurrentState() const {
  return state_;
}

void DriveFileSyncService::ApplyLocalChange(
    const fileapi::FileChange& local_file_change,
    const FilePath& local_file_path,
    const fileapi::SyncFileMetadata& local_file_metadata,
    const fileapi::FileSystemURL& url,
    const fileapi::SyncStatusCallback& callback) {
  // TODO(nhiroki): support directory operations (http://crbug.com/161442).
  DCHECK(!local_file_change.IsDirectory());

  scoped_ptr<TaskToken> token(GetToken(
      FROM_HERE, TASK_TYPE_DATABASE, "Apply local change"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::ApplyLocalChange,
        AsWeakPtr(), local_file_change, local_file_path,
        local_file_metadata, url, callback));
    return;
  }

  if (state_ == REMOTE_SERVICE_DISABLED) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(last_operation_status_, token.Pass());
    callback.Run(last_operation_status_);
    return;
  }

  switch (ResolveSyncOperationType(local_file_change, url)) {
    case SYNC_OPERATION_UPLOAD_NEW_FILE: {
      sync_client_->UploadNewFile(
          metadata_store_->GetResourceIdForOrigin(url.origin()),
          local_file_path,
          net::EscapePath(url.path().AsUTF8Unsafe()),
          local_file_metadata.size,
          base::Bind(&DriveFileSyncService::DidUploadNewFile,
                     AsWeakPtr(), base::Passed(&token), url, callback));
      return;
    }
    case SYNC_OPERATION_UPLOAD_EXISTING_FILE: {
      DriveMetadata metadata;
      const fileapi::SyncStatusCode status =
          metadata_store_->ReadEntry(url, &metadata);
      DCHECK_EQ(fileapi::SYNC_STATUS_OK, status);
      sync_client_->UploadExistingFile(
          metadata.resource_id(),
          metadata.md5_checksum(),
          local_file_path,
          local_file_metadata.size,
          base::Bind(&DriveFileSyncService::DidUploadExistingFile,
                     AsWeakPtr(), base::Passed(&token), url, callback));
      return;
    }
    case SYNC_OPERATION_DELETE_FILE: {
      DriveMetadata metadata;
      const fileapi::SyncStatusCode status =
          metadata_store_->ReadEntry(url, &metadata);
      DCHECK_EQ(fileapi::SYNC_STATUS_OK, status);
      sync_client_->DeleteFile(
          metadata.resource_id(),
          metadata.md5_checksum(),
          base::Bind(&DriveFileSyncService::DidDeleteFile,
                     AsWeakPtr(),
                     base::Passed(&token), url, callback));
      return;
    }
    case SYNC_OPERATION_IGNORE: {
      // We cannot call DidApplyLocalChange here since we should not cancel
      // the remote pending change.
      NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
      callback.Run(fileapi::SYNC_STATUS_OK);
      return;
    }
    case SYNC_OPERATION_CONFLICT: {
      // Mark the file as conflicted.
      DriveMetadata metadata;
      metadata_store_->ReadEntry(url, &metadata);
      metadata.set_conflicted(true);
      metadata_store_->UpdateEntry(
          url, metadata,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&token), url,
                     google_apis::HTTP_CONFLICT, callback));
      return;
    }
    case SYNC_OPERATION_FAILED: {
      DidApplyLocalChange(token.Pass(), url, google_apis::GDATA_OTHER_ERROR,
                          callback, fileapi::SYNC_STATUS_FAILED);
      return;
    }
  }
  NOTREACHED();
  DidApplyLocalChange(token.Pass(), url, google_apis::GDATA_OTHER_ERROR,
                      callback, fileapi::SYNC_STATUS_FAILED);
}

// Called by CreateForTesting.
DriveFileSyncService::DriveFileSyncService(
    scoped_ptr<DriveFileSyncClient> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store)
    : last_operation_status_(fileapi::SYNC_STATUS_OK),
      state_(REMOTE_SERVICE_OK),
      largest_changestamp_(0),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  token_.reset(new TaskToken(AsWeakPtr()));
  sync_client_ = sync_client.Pass();
  metadata_store_ = metadata_store.Pass();

  DidInitializeMetadataStore(
      GetToken(FROM_HERE, TASK_TYPE_NONE, "Drive initialization for testing"),
      fileapi::SYNC_STATUS_OK, false);
}

scoped_ptr<DriveFileSyncService::TaskToken> DriveFileSyncService::GetToken(
    const tracked_objects::Location& from_here,
    TaskType task_type,
    const std::string& description) {
  if (!token_)
    return scoped_ptr<TaskToken>();
  token_->UpdateTask(from_here, task_type, description);
  return token_.Pass();
}

void DriveFileSyncService::NotifyTaskDone(fileapi::SyncStatusCode status,
                                          scoped_ptr<TaskToken> token) {
  DCHECK(token);
  last_operation_status_ = status;
  token_ = token.Pass();

  if (token_->task_type() != TASK_TYPE_NONE) {
    RemoteServiceState old_state = state_;
    UpdateServiceState();

    // Notify remote sync service state for healthy running updates (OK to OK
    // state transition) and for any state changes.
    if ((state_ == REMOTE_SERVICE_OK && !token_->description().empty()) ||
        old_state != state_) {
      FOR_EACH_OBSERVER(Observer, observers_,
                        OnRemoteServiceStateUpdated(state_,
                                                    token_->description()));
    }
  }

  token_->ResetTask(FROM_HERE);
  if (status == fileapi::SYNC_STATUS_OK && !pending_tasks_.empty()) {
    base::Closure closure = pending_tasks_.front();
    pending_tasks_.pop_front();
    closure.Run();
  }
}

void DriveFileSyncService::UpdateServiceState() {
  switch (last_operation_status_) {
    // Possible regular operation errors.
    case fileapi::SYNC_STATUS_OK:
    case fileapi::SYNC_STATUS_FILE_BUSY:
    case fileapi::SYNC_STATUS_HAS_CONFLICT:
    case fileapi::SYNC_STATUS_NOT_A_CONFLICT:
    case fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC:
    case fileapi::SYNC_FILE_ERROR_NOT_FOUND:
    case fileapi::SYNC_FILE_ERROR_FAILED:
    case fileapi::SYNC_FILE_ERROR_NO_SPACE:
      // If the service type was DRIVE and the status was ok, the state
      // should be migrated to OK state.
      if (token_->task_type() == TASK_TYPE_DRIVE)
        state_ = REMOTE_SERVICE_OK;
      break;

    // Authentication error.
    case fileapi::SYNC_STATUS_AUTHENTICATION_FAILED:
      state_ = REMOTE_SERVICE_AUTHENTICATION_REQUIRED;
      break;

    // Errors which could make the service temporarily unavailable.
    case fileapi::SYNC_STATUS_RETRY:
    case fileapi::SYNC_STATUS_NETWORK_ERROR:
      state_ = REMOTE_SERVICE_TEMPORARY_UNAVAILABLE;
      break;

    // Errors which would require manual user intervention to resolve.
    case fileapi::SYNC_DATABASE_ERROR_CORRUPTION:
    case fileapi::SYNC_DATABASE_ERROR_IO_ERROR:
    case fileapi::SYNC_DATABASE_ERROR_FAILED:
    case fileapi::SYNC_STATUS_ABORT:
    case fileapi::SYNC_STATUS_FAILED:
      state_ = REMOTE_SERVICE_DISABLED;
      break;

    // Unexpected status code. They should be explicitly added to one of the
    // above three cases.
    default:
      NOTREACHED();
      state_ = REMOTE_SERVICE_DISABLED;
      break;
  }
}

base::WeakPtr<DriveFileSyncService> DriveFileSyncService::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DriveFileSyncService::DidInitializeMetadataStore(
    scoped_ptr<TaskToken> token,
    fileapi::SyncStatusCode status,
    bool created) {
  if (status != fileapi::SYNC_STATUS_OK) {
    NotifyTaskDone(status, token.Pass());
    return;
  }

  if (metadata_store_->sync_root_directory().empty()) {
    DCHECK(metadata_store_->batch_sync_origins().empty());
    DCHECK(metadata_store_->incremental_sync_origins().empty());

    token->UpdateTask(FROM_HERE, TASK_TYPE_DRIVE, "Retrieving drive root");
    sync_client_->GetDriveDirectoryForSyncRoot(
        base::Bind(&DriveFileSyncService::DidGetSyncRootDirectory,
                   AsWeakPtr(), base::Passed(&token)));
    return;
  }
  NotifyTaskDone(status, token.Pass());

  for (std::map<GURL, std::string>::const_iterator itr =
           metadata_store_->batch_sync_origins().begin();
       itr != metadata_store_->batch_sync_origins().end();
       ++itr) {
    StartBatchSyncForOrigin(itr->first, itr->second);
  }
}

void DriveFileSyncService::DidGetSyncRootDirectory(
    scoped_ptr<TaskToken> token,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    NotifyTaskDone(GDataErrorCodeToSyncStatusCode(error), token.Pass());
    return;
  }

  metadata_store_->SetSyncRootDirectory(resource_id);
  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
}

void DriveFileSyncService::StartBatchSyncForOrigin(
    const GURL& origin,
    const std::string& resource_id) {
  scoped_ptr<TaskToken> token(
      GetToken(FROM_HERE, TASK_TYPE_DRIVE, "Retrieving largest changestamp"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::StartBatchSyncForOrigin,
        AsWeakPtr(), origin, resource_id));
    return;
  }

  sync_client_->GetLargestChangeStamp(
      base::Bind(&DriveFileSyncService::DidGetLargestChangeStampForBatchSync,
                 AsWeakPtr(), base::Passed(&token), origin, resource_id));
}

void DriveFileSyncService::DidGetDirectoryForOrigin(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    fileapi::SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
    NotifyTaskDone(status, token.Pass());
    callback.Run(status);
    return;
  }

  metadata_store_->AddBatchSyncOrigin(origin, resource_id);

  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
  callback.Run(fileapi::SYNC_STATUS_OK);

  StartBatchSyncForOrigin(origin, resource_id);
}

void DriveFileSyncService::DidGetLargestChangeStampForBatchSync(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    const std::string& resource_id,
    google_apis::GDataErrorCode error,
    int64 largest_changestamp) {
  if (error != google_apis::HTTP_SUCCESS) {
    // TODO(tzik): Refine this error code.
    NotifyTaskDone(GDataErrorCodeToSyncStatusCode(error), token.Pass());
    return;
  }

  DCHECK(token);
  token->UpdateTask(FROM_HERE, TASK_TYPE_DRIVE, "Retrieving remote files");
  sync_client_->ListFiles(
      resource_id,
      base::Bind(
          &DriveFileSyncService::DidGetDirectoryContentForBatchSync,
          AsWeakPtr(), base::Passed(&token), origin, largest_changestamp));
}

void DriveFileSyncService::DidGetDirectoryContentForBatchSync(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    int64 largest_changestamp,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentFeed> feed) {
  if (error != google_apis::HTTP_SUCCESS) {
    // TODO(tzik): Refine this error code.
    NotifyTaskDone(GDataErrorCodeToSyncStatusCode(error), token.Pass());
    return;
  }

  typedef ScopedVector<google_apis::DocumentEntry>::const_iterator iterator;
  for (iterator itr = feed->entries().begin();
       itr != feed->entries().end(); ++itr) {
    AppendNewRemoteChange(origin, *itr, largest_changestamp);
  }

  GURL next_feed_url;
  if (feed->GetNextFeedURL(&next_feed_url)) {
    sync_client_->ContinueListing(
        next_feed_url,
        base::Bind(
            &DriveFileSyncService::DidGetDirectoryContentForBatchSync,
            AsWeakPtr(), base::Passed(&token), origin, largest_changestamp));
    return;
  }
  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
}

void DriveFileSyncService::DidRemoveOriginOnMetadataStore(
    scoped_ptr<TaskToken> token,
    const fileapi::SyncStatusCallback& callback,
    fileapi::SyncStatusCode status) {
  NotifyTaskDone(status, token.Pass());
  callback.Run(status);
}

void DriveFileSyncService::DidGetRemoteFileMetadata(
    const fileapi::SyncFileMetadataCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentEntry> entry) {
  fileapi::SyncFileType file_type = fileapi::SYNC_FILE_TYPE_UNKNOWN;
  if (entry->is_file())
    file_type = fileapi::SYNC_FILE_TYPE_FILE;
  else if (entry->is_folder())
    file_type = fileapi::SYNC_FILE_TYPE_DIRECTORY;
  callback.Run(GDataErrorCodeToSyncStatusCode(error),
               fileapi::SyncFileMetadata(file_type,
                                         entry->file_size(),
                                         entry->updated_time()));
}

DriveFileSyncService::SyncOperationType
DriveFileSyncService::ResolveSyncOperationType(
    const fileapi::FileChange& local_file_change,
    const fileapi::FileSystemURL& url) {
  // TODO(nhiroki): check metadata.conflicted() flag before checking the pending
  // change queue (http://crbug.com/162798).

  URLToChange::const_iterator found_url = url_to_change_.find(url.origin());
  if (found_url != url_to_change_.end()) {
    const PathToChange& path_to_change = found_url->second;
    PathToChange::const_iterator found_path = path_to_change.find(url.path());
    if (found_path != path_to_change.end()) {
      // Remote change for the file identified by |url| exists in the pending
      // change queue.
      const fileapi::FileChange& remote_file_change = found_path->second.change;

      // (RemoteChange) + (LocalChange) -> (Operation Type)
      // AddOrUpdate    + AddOrUpdate   -> CONFLICT
      // AddOrUpdate    + Delete        -> IGNORE
      // Delete         + AddOrUpdate   -> UPLOAD_NEW_FILE
      // Delete         + Delete        -> IGNORE

      if (remote_file_change.IsAddOrUpdate() &&
          local_file_change.IsAddOrUpdate())
        return SYNC_OPERATION_CONFLICT;
      else if (remote_file_change.IsDelete() &&
               local_file_change.IsAddOrUpdate())
        return SYNC_OPERATION_UPLOAD_NEW_FILE;
      else if (local_file_change.IsDelete())
        return SYNC_OPERATION_IGNORE;
      NOTREACHED();
      return SYNC_OPERATION_FAILED;
    }
  }

  DriveMetadata metadata;
  if (metadata_store_->ReadEntry(url, &metadata) == fileapi::SYNC_STATUS_OK) {
    // The remote file identified by |url| exists on Drive.
    switch (local_file_change.change()) {
      case fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE:
        return SYNC_OPERATION_UPLOAD_EXISTING_FILE;
      case fileapi::FileChange::FILE_CHANGE_DELETE:
        return SYNC_OPERATION_DELETE_FILE;
    }
    NOTREACHED();
    return SYNC_OPERATION_FAILED;
  }

  // The remote file identified by |url| does not exist on Drive.
  switch (local_file_change.change()) {
    case fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      return SYNC_OPERATION_UPLOAD_NEW_FILE;
    case fileapi::FileChange::FILE_CHANGE_DELETE:
      return SYNC_OPERATION_IGNORE;
  }
  NOTREACHED();
  return SYNC_OPERATION_FAILED;
}

void DriveFileSyncService::DidApplyLocalChange(
    scoped_ptr<TaskToken> token,
    const fileapi::FileSystemURL& url,
    const google_apis::GDataErrorCode error,
    const fileapi::SyncStatusCallback& callback,
    fileapi::SyncStatusCode status) {
  if (status == fileapi::SYNC_STATUS_OK) {
    CancelRemoteChange(url);
    NotifyTaskDone(GDataErrorCodeToSyncStatusCode(error), token.Pass());
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }
  NotifyTaskDone(status, token.Pass());
  callback.Run(status);
}

void DriveFileSyncService::DidUploadNewFile(
    scoped_ptr<TaskToken> token,
    const fileapi::FileSystemURL& url,
    const fileapi::SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& file_md5) {
  if (error == google_apis::HTTP_SUCCESS) {
    DriveMetadata metadata;
    metadata_store_->ReadEntry(url, &metadata);
    metadata.set_resource_id(resource_id);
    metadata.set_md5_checksum(file_md5);
    metadata.set_conflicted(false);
    metadata_store_->UpdateEntry(
        url, metadata,
        base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                   AsWeakPtr(), base::Passed(&token), url, error, callback));
    return;
  }
  const fileapi::SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  NotifyTaskDone(status, token.Pass());
  callback.Run(status);
}

void DriveFileSyncService::DidUploadExistingFile(
    scoped_ptr<TaskToken> token,
    const fileapi::FileSystemURL& url,
    const fileapi::SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& file_md5) {
  switch (error) {
    case google_apis::HTTP_SUCCESS: {
      DriveMetadata metadata;
      metadata_store_->ReadEntry(url, &metadata);
      metadata.set_resource_id(resource_id);
      metadata.set_md5_checksum(file_md5);
      metadata.set_conflicted(false);
      metadata_store_->UpdateEntry(
          url, metadata,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&token), url, error, callback));
      return;
    }
    case google_apis::HTTP_CONFLICT: {
      // Mark the file as conflicted.
      DriveMetadata metadata;
      metadata_store_->ReadEntry(url, &metadata);
      metadata.set_conflicted(true);
      metadata_store_->UpdateEntry(
          url, metadata,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&token), url, error, callback));
      return;
    }
    case google_apis::HTTP_NOT_MODIFIED: {
      DidApplyLocalChange(token.Pass(), url, google_apis::HTTP_SUCCESS,
                          callback, fileapi::SYNC_STATUS_OK);
      return;
    }
    default: {
      const fileapi::SyncStatusCode status =
          GDataErrorCodeToSyncStatusCode(error);
      DCHECK_NE(fileapi::SYNC_STATUS_OK, status);
      DidApplyLocalChange(token.Pass(), url, error, callback, status);
      return;
    }
  }
}

void DriveFileSyncService::DidDeleteFile(
    scoped_ptr<TaskToken> token,
    const fileapi::FileSystemURL& url,
    const fileapi::SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  switch (error) {
    case google_apis::HTTP_SUCCESS: {
      metadata_store_->DeleteEntry(
          url,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&token), url, error, callback));
      return;
    }
    case google_apis::HTTP_CONFLICT: {
      // Mark the file as conflicted.
      DriveMetadata metadata;
      metadata_store_->ReadEntry(url, &metadata);
      metadata.set_conflicted(true);
      metadata_store_->UpdateEntry(
          url, metadata,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&token), url, error, callback));
      return;
    }
    default: {
      const fileapi::SyncStatusCode status =
          GDataErrorCodeToSyncStatusCode(error);
      DCHECK_NE(fileapi::SYNC_STATUS_OK, status);
      DidApplyLocalChange(token.Pass(), url, error, callback, status);
      return;
    }
  }
}

void DriveFileSyncService::AppendNewRemoteChange(
    const GURL& origin,
    google_apis::DocumentEntry* entry,
    int64 changestamp) {
  FilePath path = FilePath::FromUTF8Unsafe(UTF16ToUTF8(entry->title()));

  PathToChange* path_to_change = &url_to_change_[origin];
  PathToChange::iterator found = path_to_change->find(path);
  if (found != path_to_change->end()) {
    if (found->second.changestamp >= changestamp)
      return;
    pending_changes_.erase(found->second.position_in_queue);
  }

  fileapi::FileSystemURL url(
      fileapi::CreateSyncableFileSystemURL(origin, kServiceName, path));

  fileapi::FileChange::ChangeType change_type;
  fileapi::SyncFileType file_type;
  if (entry->deleted()) {
    change_type = fileapi::FileChange::FILE_CHANGE_DELETE;
    file_type = fileapi::SYNC_FILE_TYPE_UNKNOWN;
  } else {
    change_type = fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE;
    if (entry->kind() == google_apis::ENTRY_KIND_FOLDER)
      file_type = fileapi::SYNC_FILE_TYPE_DIRECTORY;
    else
      file_type = fileapi::SYNC_FILE_TYPE_FILE;
  }

  fileapi::FileChange file_change(change_type, file_type);

  ChangeQueueItem item(changestamp, url);
  std::pair<PendingChangeQueue::iterator, bool> inserted_to_queue =
      pending_changes_.insert(ChangeQueueItem(changestamp, url));
  DCHECK(inserted_to_queue.second);

  (*path_to_change)[path] = RemoteChange(
      changestamp, entry->resource_id(), url, file_change,
      inserted_to_queue.first);
}

void DriveFileSyncService::CancelRemoteChange(
    const fileapi::FileSystemURL& url) {
  URLToChange::iterator found_origin = url_to_change_.find(url.origin());
  if (found_origin == url_to_change_.end())
    return;

  PathToChange* path_to_change = &found_origin->second;
  PathToChange::iterator found_change = path_to_change->find(url.path());
  if (found_change == path_to_change->end())
    return;

  pending_changes_.erase(found_change->second.position_in_queue);
  path_to_change->erase(found_change);
  if (path_to_change->empty())
    url_to_change_.erase(found_origin);
}

}  // namespace sync_file_system

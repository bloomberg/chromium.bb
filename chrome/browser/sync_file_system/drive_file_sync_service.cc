// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_file_type.h"
#include "webkit/fileapi/syncable/sync_operation_result.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace sync_file_system {

namespace {

const FilePath::CharType kTempDirName[] = FILE_PATH_LITERAL("tmp");
const FilePath::CharType kSyncFileSystemDir[] =
    FILE_PATH_LITERAL("Sync FileSystem");
const int64 kMinimumPollingDelaySeconds = 10;
const int64 kMaximumPollingDelaySeconds = 60 * 60;  // 1 hour
const double kDelayMultiplier = 2;

bool CreateTemporaryFile(const FilePath& dir_path, FilePath* temp_file) {
  return file_util::CreateDirectory(dir_path) &&
      file_util::CreateTemporaryFileInDir(dir_path, temp_file);
}

void DeleteTemporaryFile(const FilePath& file_path) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&DeleteTemporaryFile, file_path));
    return;
  }

  if (!file_util::Delete(file_path, true))
    LOG(ERROR) << "Leaked temporary file for Sync FileSystem: "
               << file_path.value();
}

void DidUpdateConflictState(fileapi::SyncStatusCode status) {
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, status);
}

void EmptyStatusCallback(fileapi::SyncStatusCode code) {}

void EnablePolling(bool* polling_enabled) {
  *polling_enabled = true;
}

}  // namespace

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
    completion_callback_.Reset();
  }

  void UpdateTask(const tracked_objects::Location& location,
                  TaskType task_type,
                  const std::string& description) {
    location_ = location;
    task_type_ = task_type;
    description_ = description;

    DVLOG(2) << "Token updated: " << description_
             << " " << location_.ToString();
  }

  const tracked_objects::Location& location() const { return location_; }
  TaskType task_type() const { return task_type_; }
  const std::string& description() const { return description_; }
  std::string done_description() const { return description_ + " done"; }

  void set_completion_callback(const base::Closure& callback) {
    completion_callback_ = callback;
  }

  const base::Closure& completion_callback() {
    return completion_callback_;
  }

  ~TaskToken() {
    // All task on DriveFileSyncService must hold TaskToken instance to ensure
    // no other tasks are running. Also, as soon as a task finishes to work,
    // it must return the token to DriveFileSyncService.
    // Destroying a token with valid |sync_service_| indicates the token was
    // dropped by a task without returning.
    if (sync_service_) {
      LOG(ERROR) << "Unexpected TaskToken deletion from: "
                 << location_.ToString() << " while: " << description_;
    }
    DCHECK(!sync_service_);
  }

 private:
  base::WeakPtr<DriveFileSyncService> sync_service_;
  tracked_objects::Location location_;
  TaskType task_type_;
  std::string description_;
  base::Closure completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

struct DriveFileSyncService::ProcessRemoteChangeParam {
  scoped_ptr<TaskToken> token;
  RemoteChangeProcessor* processor;
  RemoteChange remote_change;
  fileapi::SyncOperationCallback callback;

  DriveMetadata drive_metadata;
  bool metadata_updated;
  FilePath temporary_file_path;
  std::string md5_checksum;
  fileapi::SyncOperationResult operation_result;
  bool clear_local_changes;

  ProcessRemoteChangeParam(scoped_ptr<TaskToken> token,
                           RemoteChangeProcessor* processor,
                           const RemoteChange& remote_change,
                           const fileapi::SyncOperationCallback& callback)
      : token(token.Pass()),
        processor(processor),
        remote_change(remote_change),
        callback(callback),
        metadata_updated(false),
        operation_result(fileapi::SYNC_OPERATION_NONE),
        clear_local_changes(true) {
  }
};

DriveFileSyncService::ChangeQueueItem::ChangeQueueItem()
    : changestamp(0),
      sync_type(REMOTE_SYNC_TYPE_INCREMENTAL) {
}

DriveFileSyncService::ChangeQueueItem::ChangeQueueItem(
    int64 changestamp,
    RemoteSyncType sync_type,
    const fileapi::FileSystemURL& url)
    : changestamp(changestamp),
      sync_type(sync_type),
      url(url) {
}

bool DriveFileSyncService::ChangeQueueComparator::operator()(
    const ChangeQueueItem& left,
    const ChangeQueueItem& right) {
  if (left.changestamp != right.changestamp)
    return left.changestamp < right.changestamp;
  if (left.sync_type != right.sync_type)
    return left.sync_type < right.sync_type;
  return fileapi::FileSystemURL::Comparator()(left.url, right.url);
}

DriveFileSyncService::RemoteChange::RemoteChange()
    : changestamp(0),
      sync_type(REMOTE_SYNC_TYPE_INCREMENTAL),
      change(fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE,
             fileapi::SYNC_FILE_TYPE_UNKNOWN) {
}

DriveFileSyncService::RemoteChange::RemoteChange(
    int64 changestamp,
    const std::string& resource_id,
    RemoteSyncType sync_type,
    const fileapi::FileSystemURL& url,
    const fileapi::FileChange& change,
    PendingChangeQueue::iterator position_in_queue)
    : changestamp(changestamp),
      resource_id(resource_id),
      sync_type(sync_type),
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
      largest_fetched_changestamp_(0),
      polling_delay_seconds_(kMinimumPollingDelaySeconds),
      polling_enabled_(true),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  temporary_file_dir_ =
      profile->GetPath().Append(kSyncFileSystemDir).Append(kTempDirName);
  token_.reset(new TaskToken(AsWeakPtr()));

  sync_client_.reset(new DriveFileSyncClient(profile));
  sync_client_->AddObserver(this);

  metadata_store_.reset(new DriveMetadataStore(
      profile->GetPath().Append(kSyncFileSystemDir),
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
  sync_client_->RemoveObserver(this);
  token_.reset();
}

// static
scoped_ptr<DriveFileSyncService> DriveFileSyncService::CreateForTesting(
    const FilePath& base_dir,
    scoped_ptr<DriveFileSyncClient> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store) {
  return make_scoped_ptr(new DriveFileSyncService(
      base_dir, sync_client.Pass(), metadata_store.Pass()));
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
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));
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

  if (metadata_store_->sync_root_directory().empty()) {
    GetSyncRootDirectory(
        token.Pass(),
        base::Bind(&DriveFileSyncService::DidGetSyncRootForRegisterOrigin,
                   AsWeakPtr(), origin, callback));
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
  scoped_ptr<TaskToken> token(
      GetToken(FROM_HERE, TASK_TYPE_DRIVE, "Process remote change"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::ProcessRemoteChange, AsWeakPtr(),
        processor, callback));
    return;
  }

  if (state_ == REMOTE_SERVICE_DISABLED) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(last_operation_status_, token.Pass());
    callback.Run(last_operation_status_, fileapi::FileSystemURL(),
                 fileapi::SYNC_OPERATION_NONE);
    return;
  }

  if (pending_changes_.empty()) {
    NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
    callback.Run(fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC,
                 fileapi::FileSystemURL(),
                 fileapi::SYNC_OPERATION_NONE);
    return;
  }

  const fileapi::FileSystemURL& url = pending_changes_.begin()->url;
  const GURL& origin = url.origin();
  const FilePath& path = url.path();
  DCHECK(ContainsKey(url_to_change_, origin));
  PathToChange* path_to_change = &url_to_change_[origin];
  DCHECK(ContainsKey(*path_to_change, path));
  const RemoteChange& remote_change = (*path_to_change)[path];

  DVLOG(1) << "ProcessRemoteChange for " << url.DebugString()
           << " remote_change:" << remote_change.change.DebugString();

  scoped_ptr<ProcessRemoteChangeParam> param(new ProcessRemoteChangeParam(
      token.Pass(), processor, remote_change, callback));
  processor->PrepareForProcessRemoteChange(
      remote_change.url,
      base::Bind(&DriveFileSyncService::DidPrepareForProcessRemoteChange,
                 AsWeakPtr(), base::Passed(&param)));
}

LocalChangeProcessor* DriveFileSyncService::GetLocalChangeProcessor() {
  return this;
}

bool DriveFileSyncService::IsConflicting(const fileapi::FileSystemURL& url) {
  DriveMetadata metadata;
  const fileapi::SyncStatusCode status =
      metadata_store_->ReadEntry(url, &metadata);
  if (status != fileapi::SYNC_STATUS_OK) {
    DCHECK_EQ(fileapi::SYNC_DATABASE_ERROR_NOT_FOUND, status);
    return false;
  }
  return metadata.conflicted();
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

const char* DriveFileSyncService::GetServiceName() const {
  return kServiceName;
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

  DriveFileSyncService::SyncOperationType operation =
      ResolveSyncOperationType(local_file_change, url);

  DVLOG(1) << "ApplyLocalChange for " << url.DebugString()
           << " local_change:" << local_file_change.DebugString()
           << " ==> operation:" << operation;

  switch (operation) {
    case SYNC_OPERATION_UPLOAD_NEW_FILE: {
      sync_client_->UploadNewFile(
          metadata_store_->GetResourceIdForOrigin(url.origin()),
          local_file_path,
          url.path().AsUTF8Unsafe(),
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
      const fileapi::SyncStatusCode status =
          metadata_store_->ReadEntry(url, &metadata);
      if (status == fileapi::SYNC_DATABASE_ERROR_NOT_FOUND) {
        // The file is not yet in the metadata store.
        RemoteChange remote_change;
        const bool has_remote_change = GetPendingChangeForFileSystemURL(
            url, &remote_change);
        DCHECK(has_remote_change);
        metadata.set_resource_id(remote_change.resource_id);
        metadata.set_md5_checksum(std::string());
        metadata.set_to_be_fetched(false);
      }
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

void DriveFileSyncService::OnAuthenticated() {
  DVLOG(1) << "OnAuthenticated";
  if (state_ == REMOTE_SERVICE_AUTHENTICATION_REQUIRED ||
      state_ == REMOTE_SERVICE_TEMPORARY_UNAVAILABLE) {
    state_ = REMOTE_SERVICE_OK;
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OnRemoteServiceStateUpdated(state_, "Authenticated"));
  }
}

void DriveFileSyncService::OnNetworkConnected() {
  DVLOG(1) << "OnNetworkConnected";
  if (state_ == REMOTE_SERVICE_AUTHENTICATION_REQUIRED ||
      state_ == REMOTE_SERVICE_TEMPORARY_UNAVAILABLE) {
    state_ = REMOTE_SERVICE_OK;
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OnRemoteServiceStateUpdated(state_, "Network connected"));
  }
}

// Called by CreateForTesting.
DriveFileSyncService::DriveFileSyncService(
    const FilePath& base_dir,
    scoped_ptr<DriveFileSyncClient> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store)
    : last_operation_status_(fileapi::SYNC_STATUS_OK),
      state_(REMOTE_SERVICE_OK),
      largest_fetched_changestamp_(0),
      polling_enabled_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  temporary_file_dir_ = base_dir.Append(kTempDirName);

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
    DVLOG(2) << "NotifyTaskDone: " << token_->description()
             << ": finished with status=" << status
             << " (" << SyncStatusCodeToString(status) << ")"
             << " " << token_->location().ToString();

    RemoteServiceState old_state = state_;
    UpdateServiceState();

    // Notify remote sync service state if the state has been changed.
    if (!token_->description().empty() || old_state != state_) {
      FOR_EACH_OBSERVER(
          Observer, observers_,
          OnRemoteServiceStateUpdated(state_, token_->done_description()));
    }
  }

  if (!token_->completion_callback().is_null())
    token_->completion_callback().Run();

  token_->ResetTask(FROM_HERE);
  if (!pending_tasks_.empty()) {
    base::Closure closure = pending_tasks_.front();
    pending_tasks_.pop_front();
    closure.Run();
    return;
  }

  SchedulePolling();

  if (state_ != REMOTE_SERVICE_OK)
    return;

  // If the state has become OK and we have any pending batch sync origins
  // restart batch sync for them.
  if (!pending_batch_sync_origins_.empty()) {
    GURL origin = *pending_batch_sync_origins_.begin();
    pending_batch_sync_origins_.erase(pending_batch_sync_origins_.begin());
    std::string resource_id = metadata_store_->GetResourceIdForOrigin(origin);
    StartBatchSyncForOrigin(origin, resource_id);
    return;
  }

  // Notify observer of the update of |pending_changes_|.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRemoteChangeQueueUpdated(pending_changes_.size()));
}

void DriveFileSyncService::UpdateServiceState() {
  switch (last_operation_status_) {
    // Possible regular operation errors.
    case fileapi::SYNC_STATUS_OK:
    case fileapi::SYNC_STATUS_FILE_BUSY:
    case fileapi::SYNC_STATUS_HAS_CONFLICT:
    case fileapi::SYNC_STATUS_NO_CONFLICT:
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

  largest_fetched_changestamp_ = metadata_store_->GetLargestChangeStamp();

  if (metadata_store_->sync_root_directory().empty()) {
    GetSyncRootDirectory(token.Pass(), base::Bind(&EmptyStatusCallback));
    return;
  }

  DriveMetadataStore::URLAndResourceIdList to_be_fetched_files;
  status = metadata_store_->GetToBeFetchedFiles(&to_be_fetched_files);
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, status);
  typedef DriveMetadataStore::URLAndResourceIdList::const_iterator iterator;
  for (iterator itr = to_be_fetched_files.begin();
       itr != to_be_fetched_files.end(); ++itr) {
    DriveMetadata metadata;
    const fileapi::FileSystemURL& url = itr->first;
    const std::string& resource_id = itr->second;
    AppendFetchChange(url.origin(), url.path(), resource_id);
  }

  NotifyTaskDone(status, token.Pass());

  for (std::map<GURL, std::string>::const_iterator itr =
           metadata_store_->batch_sync_origins().begin();
       itr != metadata_store_->batch_sync_origins().end();
       ++itr) {
    StartBatchSyncForOrigin(itr->first, itr->second);
  }
}

void DriveFileSyncService::GetSyncRootDirectory(
    scoped_ptr<TaskToken> token,
    const fileapi::SyncStatusCallback& callback) {
  DCHECK(metadata_store_->sync_root_directory().empty());
  DCHECK(metadata_store_->batch_sync_origins().empty());
  DCHECK(metadata_store_->incremental_sync_origins().empty());

  token->UpdateTask(FROM_HERE, TASK_TYPE_DRIVE, "Retrieving drive root");
  sync_client_->GetDriveDirectoryForSyncRoot(
      base::Bind(&DriveFileSyncService::DidGetSyncRootDirectory,
                  AsWeakPtr(), base::Passed(&token), callback));
}

void DriveFileSyncService::DidGetSyncRootDirectory(
    scoped_ptr<TaskToken> token,
    const fileapi::SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  fileapi::SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    NotifyTaskDone(status, token.Pass());
    callback.Run(status);
    return;
  }

  metadata_store_->SetSyncRootDirectory(resource_id);
  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
  callback.Run(status);
}

void DriveFileSyncService::DidGetSyncRootForRegisterOrigin(
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback,
    fileapi::SyncStatusCode status) {
  if (status != fileapi::SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }
  RegisterOriginForTrackingChanges(origin, callback);
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
    fileapi::SyncStatusCode status =
        GDataErrorCodeToSyncStatusCodeWrapper(error);
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
    pending_batch_sync_origins_.insert(origin);
    // TODO(tzik): Refine this error code.
    NotifyTaskDone(GDataErrorCodeToSyncStatusCodeWrapper(error), token.Pass());
    return;
  }

  if (metadata_store_->incremental_sync_origins().empty()) {
    largest_fetched_changestamp_ = largest_changestamp;
    metadata_store_->SetLargestChangeStamp(
        largest_changestamp,
        base::Bind(&EmptyStatusCallback));
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
    pending_batch_sync_origins_.insert(origin);
    // TODO(tzik): Refine this error code.
    NotifyTaskDone(GDataErrorCodeToSyncStatusCodeWrapper(error), token.Pass());
    return;
  }

  typedef ScopedVector<google_apis::DocumentEntry>::const_iterator iterator;
  for (iterator itr = feed->entries().begin();
       itr != feed->entries().end(); ++itr) {
    AppendRemoteChange(origin, **itr, largest_changestamp,
                       REMOTE_SYNC_TYPE_BATCH);
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
  callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error),
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

  RemoteChange remote_change;
  const bool has_remote_change = GetPendingChangeForFileSystemURL(
      url, &remote_change);

  if (has_remote_change) {
    // Remote change for the file identified by |url| exists in the pending
    // change queue.
    const fileapi::FileChange& remote_file_change = remote_change.change;

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
    RemoveRemoteChange(url);
    NotifyTaskDone(GDataErrorCodeToSyncStatusCodeWrapper(error), token.Pass());
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
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
    metadata.set_resource_id(resource_id);
    metadata.set_md5_checksum(file_md5);
    metadata.set_conflicted(false);
    metadata.set_to_be_fetched(false);
    metadata_store_->UpdateEntry(
        url, metadata,
        base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                   AsWeakPtr(), base::Passed(&token), url, error, callback));
    return;
  }
  const fileapi::SyncStatusCode status =
      GDataErrorCodeToSyncStatusCodeWrapper(error);
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
      metadata.set_resource_id(resource_id);
      metadata.set_md5_checksum(file_md5);
      metadata.set_conflicted(false);
      metadata.set_to_be_fetched(false);
      metadata_store_->UpdateEntry(
          url, metadata,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&token), url, error, callback));
      return;
    }
    case google_apis::HTTP_CONFLICT: {
      // Mark the file as conflicted.
      DriveMetadata metadata;
      const fileapi::SyncStatusCode status =
          metadata_store_->ReadEntry(url, &metadata);
      DCHECK_EQ(fileapi::SYNC_STATUS_OK, status);
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
          GDataErrorCodeToSyncStatusCodeWrapper(error);
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
      const fileapi::SyncStatusCode status =
          metadata_store_->ReadEntry(url, &metadata);
      DCHECK_EQ(fileapi::SYNC_STATUS_OK, status);
      metadata.set_conflicted(true);
      metadata_store_->UpdateEntry(
          url, metadata,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&token), url, error, callback));
      return;
    }
    default: {
      const fileapi::SyncStatusCode status =
          GDataErrorCodeToSyncStatusCodeWrapper(error);
      DCHECK_NE(fileapi::SYNC_STATUS_OK, status);
      DidApplyLocalChange(token.Pass(), url, error, callback, status);
      return;
    }
  }
}

void DriveFileSyncService::DidPrepareForProcessRemoteChange(
    scoped_ptr<ProcessRemoteChangeParam> param,
    fileapi::SyncStatusCode status,
    const fileapi::SyncFileMetadata& metadata,
    const fileapi::FileChangeList& local_changes) {
  if (status != fileapi::SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  const fileapi::FileSystemURL& url = param->remote_change.url;
  const DriveMetadata& drive_metadata = param->drive_metadata;
  const fileapi::FileChange& remote_file_change = param->remote_change.change;

  status = metadata_store_->ReadEntry(param->remote_change.url,
                                      &param->drive_metadata);
  DCHECK(status == fileapi::SYNC_STATUS_OK ||
         status == fileapi::SYNC_DATABASE_ERROR_NOT_FOUND);

  bool missing_db_entry = (status != fileapi::SYNC_STATUS_OK);
  if (missing_db_entry) {
    param->drive_metadata.set_resource_id(param->remote_change.resource_id);
    param->drive_metadata.set_md5_checksum(std::string());
    param->drive_metadata.set_conflicted(false);
    param->drive_metadata.set_to_be_fetched(false);
  }
  bool missing_local_file =
      (metadata.file_type == fileapi::SYNC_FILE_TYPE_UNKNOWN);

  if (param->drive_metadata.conflicted()) {
    if (missing_local_file) {
      if (remote_file_change.IsAddOrUpdate()) {
        DVLOG(1) << "ProcessRemoteChange for " << url.DebugString()
                 << (param->drive_metadata.conflicted() ? " (conflicted)" : " ")
                 << (missing_local_file ? " (missing local file)" : " ")
                 << " remote_change: " << remote_file_change.DebugString()
                 << " ==> operation: ResolveConflictToRemoteChange";
        NOTIMPLEMENTED() << "ResolveConflictToRemoteChange()";
        AbortRemoteSync(param.Pass(), fileapi::SYNC_STATUS_FAILED);
        return;
      }

      DCHECK(remote_file_change.IsDelete());
      param->operation_result = fileapi::SYNC_OPERATION_NONE;
      DeleteMetadataForRemoteSync(param.Pass());
      return;
    }

    DCHECK(!missing_local_file);
    if (remote_file_change.IsAddOrUpdate()) {
      param->operation_result = fileapi::SYNC_OPERATION_NONE;
      param->drive_metadata.set_conflicted(true);

      metadata_store_->UpdateEntry(
          url, drive_metadata,
          base::Bind(&DriveFileSyncService::CompleteRemoteSync, AsWeakPtr(),
                     base::Passed(&param)));
      return;
    }

    DCHECK(remote_file_change.IsDelete());
    DVLOG(1) << "ProcessRemoteChange for " << url.DebugString()
             << (param->drive_metadata.conflicted() ? " (conflicted)" : " ")
             << (missing_local_file ? " (missing local file)" : " ")
             << " remote_change: " << remote_file_change.DebugString()
             << " ==> operation: ResolveConflictToLocalChange";

    param->operation_result = fileapi::SYNC_OPERATION_NONE;
    param->clear_local_changes = false;

    RemoteChangeProcessor* processor = param->processor;
    ResolveConflictToLocalChange(
        processor, url,
        base::Bind(&DriveFileSyncService::DidResolveConflictToLocalChange,
                   AsWeakPtr(), base::Passed(&param)));
    return;
  }

  DCHECK(!param->drive_metadata.conflicted());
  if (remote_file_change.IsAddOrUpdate()) {
    if (local_changes.empty()) {
      if (missing_local_file) {
        param->operation_result = fileapi::SYNC_OPERATION_ADDED;
        DownloadForRemoteSync(param.Pass());
        return;
      }

      DCHECK(!missing_local_file);
      param->operation_result = fileapi::SYNC_OPERATION_UPDATED;
      DownloadForRemoteSync(param.Pass());
      return;
    }

    DCHECK(!local_changes.empty());
    if (local_changes.list().back().IsAddOrUpdate()) {
      param->operation_result = fileapi::SYNC_OPERATION_CONFLICTED;
      param->drive_metadata.set_conflicted(true);

      metadata_store_->UpdateEntry(
          url, drive_metadata,
          base::Bind(&DriveFileSyncService::CompleteRemoteSync, AsWeakPtr(),
                     base::Passed(&param)));
      return;
    }

    DCHECK(local_changes.list().back().IsDelete());
    param->operation_result = fileapi::SYNC_OPERATION_ADDED;
    DownloadForRemoteSync(param.Pass());
    return;
  }

  DCHECK(remote_file_change.IsDelete());
  if (local_changes.empty()) {
    if (missing_local_file) {
      param->operation_result = fileapi::SYNC_OPERATION_NONE;
      if (missing_db_entry)
        CompleteRemoteSync(param.Pass(), fileapi::SYNC_STATUS_OK);
      else
        DeleteMetadataForRemoteSync(param.Pass());
      return;
    }
    DCHECK(!missing_local_file);
    param->operation_result = fileapi::SYNC_OPERATION_DELETED;

    const fileapi::FileChange& file_change = remote_file_change;
    param->processor->ApplyRemoteChange(
        file_change, FilePath(), url,
        base::Bind(&DriveFileSyncService::DidApplyRemoteChange, AsWeakPtr(),
                   base::Passed(&param)));
    return;
  }

  DCHECK(!local_changes.empty());
  if (local_changes.list().back().IsAddOrUpdate()) {
    param->operation_result = fileapi::SYNC_OPERATION_NONE;
    CompleteRemoteSync(param.Pass(), fileapi::SYNC_STATUS_OK);
    return;
  }

  DCHECK(local_changes.list().back().IsDelete());
  param->operation_result = fileapi::SYNC_OPERATION_NONE;
  if (missing_db_entry)
    CompleteRemoteSync(param.Pass(), fileapi::SYNC_STATUS_OK);
  else
    DeleteMetadataForRemoteSync(param.Pass());
}

void DriveFileSyncService::ResolveConflictToLocalChange(
    RemoteChangeProcessor* processor,
    const fileapi::FileSystemURL& url,
    const fileapi::SyncStatusCallback& callback) {
  const fileapi::FileChange file_change(
      fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE,
      fileapi::SYNC_FILE_TYPE_FILE);
  processor->RecordFakeLocalChange(url, file_change, callback);
}

void DriveFileSyncService::DidResolveConflictToLocalChange(
    scoped_ptr<ProcessRemoteChangeParam> param,
    fileapi::SyncStatusCode status) {
  if (status != fileapi::SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  const DriveMetadata& metadata = param->drive_metadata;
  param->drive_metadata.set_conflicted(false);
  const fileapi::FileSystemURL& url = param->remote_change.url;

  metadata_store_->UpdateEntry(
      url, metadata,
      base::Bind(&DriveFileSyncService::CompleteRemoteSync,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DownloadForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param) {
  // TODO(tzik): Use ShareableFileReference here after we get thread-safe
  // version of it. crbug.com/162598
  FilePath* temporary_file_path = &param->temporary_file_path;
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateTemporaryFile,
                 temporary_file_dir_, temporary_file_path),
      base::Bind(&DriveFileSyncService::DidGetTemporaryFileForDownload,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DidGetTemporaryFileForDownload(
    scoped_ptr<ProcessRemoteChangeParam> param,
    bool success) {
  if (!success) {
    AbortRemoteSync(param.Pass(), fileapi::SYNC_FILE_ERROR_FAILED);
    return;
  }

  const FilePath& temporary_file_path = param->temporary_file_path;
  std::string resource_id = param->remote_change.resource_id;

  // We should not use the md5 in metadata for FETCH type to avoid the download
  // finishes due to NOT_MODIFIED.
  std::string md5_checksum;
  if (param->remote_change.sync_type != REMOTE_SYNC_TYPE_FETCH)
    md5_checksum = param->drive_metadata.md5_checksum();

  sync_client_->DownloadFile(
      resource_id, md5_checksum,
      temporary_file_path,
      base::Bind(&DriveFileSyncService::DidDownloadFile,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DidDownloadFile(
    scoped_ptr<ProcessRemoteChangeParam> param,
    google_apis::GDataErrorCode error,
    const std::string& md5_checksum) {
  if (error == google_apis::HTTP_NOT_MODIFIED) {
    param->operation_result = fileapi::SYNC_OPERATION_NONE;
    CompleteRemoteSync(param.Pass(), fileapi::SYNC_STATUS_OK);
    return;
  }

  fileapi::SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status != fileapi::SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  param->drive_metadata.set_md5_checksum(md5_checksum);
  const fileapi::FileChange& change = param->remote_change.change;
  const FilePath& temporary_file_path = param->temporary_file_path;
  const fileapi::FileSystemURL& url = param->remote_change.url;
  param->processor->ApplyRemoteChange(
      change, temporary_file_path, url,
      base::Bind(&DriveFileSyncService::DidApplyRemoteChange,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DidApplyRemoteChange(
    scoped_ptr<ProcessRemoteChangeParam> param,
    fileapi::SyncStatusCode status) {
  if (status != fileapi::SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  fileapi::FileSystemURL url = param->remote_change.url;
  if (param->remote_change.change.IsDelete()) {
    DeleteMetadataForRemoteSync(param.Pass());
    return;
  }

  const DriveMetadata& drive_metadata = param->drive_metadata;
  param->drive_metadata.set_resource_id(param->remote_change.resource_id);
  param->drive_metadata.set_conflicted(false);

  metadata_store_->UpdateEntry(
      url, drive_metadata,
      base::Bind(&DriveFileSyncService::CompleteRemoteSync,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DeleteMetadataForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param) {
  fileapi::FileSystemURL url = param->remote_change.url;
  metadata_store_->DeleteEntry(
      url,
      base::Bind(&DriveFileSyncService::CompleteRemoteSync,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::CompleteRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    fileapi::SyncStatusCode status) {
  if (status != fileapi::SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  RemoveRemoteChange(param->remote_change.url);

  if (param->drive_metadata.to_be_fetched()) {
    // Clear |to_be_fetched| flag since we completed fetching the remote change
    // and applying it to the local file.
    DCHECK(!param->drive_metadata.conflicted());
    param->drive_metadata.set_to_be_fetched(false);
    metadata_store_->UpdateEntry(
        param->remote_change.url, param->drive_metadata,
        base::Bind(&EmptyStatusCallback));
  }

  GURL origin = param->remote_change.url.origin();
  if (param->remote_change.sync_type == REMOTE_SYNC_TYPE_INCREMENTAL) {
    DCHECK(metadata_store_->IsIncrementalSyncOrigin(origin));
    int64 changestamp = param->remote_change.changestamp;
    DCHECK(changestamp);
    metadata_store_->SetLargestChangeStamp(
        changestamp,
        base::Bind(&DriveFileSyncService::FinalizeRemoteSync,
                   AsWeakPtr(), base::Passed(&param)));
    return;
  }

  if (param->drive_metadata.conflicted())
    status = fileapi::SYNC_STATUS_HAS_CONFLICT;

  FinalizeRemoteSync(param.Pass(), status);
}

void DriveFileSyncService::AbortRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    fileapi::SyncStatusCode status) {
  FinalizeRemoteSync(param.Pass(), status);
}

void DriveFileSyncService::FinalizeRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    fileapi::SyncStatusCode status) {
  // Clear the local changes. If the operation was resolve-to-local, we should
  // not clear them here since we added the fake local change to sync with the
  // remote file.
  if (param->clear_local_changes) {
    RemoteChangeProcessor* processor = param->processor;
    const fileapi::FileSystemURL& url = param->remote_change.url;
    param->clear_local_changes = false;
    processor->ClearLocalChanges(
        url, base::Bind(&DriveFileSyncService::FinalizeRemoteSync,
                        AsWeakPtr(), base::Passed(&param), status));
    return;
  }

  if (!param->temporary_file_path.empty())
    DeleteTemporaryFile(param->temporary_file_path);
  NotifyTaskDone(status, param->token.Pass());
  if (status == fileapi::SYNC_STATUS_OK ||
      status == fileapi::SYNC_STATUS_HAS_CONFLICT) {
    param->callback.Run(status, param->remote_change.url,
                        param->operation_result);
  } else {
    param->callback.Run(status, param->remote_change.url,
                        fileapi::SYNC_OPERATION_NONE);
  }
}

bool DriveFileSyncService::AppendRemoteChange(
    const GURL& origin,
    const google_apis::DocumentEntry& entry,
    int64 changestamp,
    RemoteSyncType sync_type) {
  // TODO(tzik): Normalize the path here.
  FilePath path = FilePath::FromUTF8Unsafe(UTF16ToUTF8(entry.title()));
  DCHECK(!entry.is_folder());
  return AppendRemoteChangeInternal(
      origin, path, entry.deleted(),
      entry.resource_id(), changestamp, sync_type);
}

bool DriveFileSyncService::AppendFetchChange(
    const GURL& origin,
    const FilePath& path,
    const std::string& resource_id) {
  return AppendRemoteChangeInternal(
      origin, path, false /* is_deleted */,
      resource_id, 0 /* changestamp */,
      REMOTE_SYNC_TYPE_FETCH);
}

bool DriveFileSyncService::AppendRemoteChangeInternal(
    const GURL& origin,
    const FilePath& path,
    bool is_deleted,
    const std::string& resource_id,
    int64 changestamp,
    RemoteSyncType sync_type) {
  PathToChange* path_to_change = &url_to_change_[origin];
  PathToChange::iterator found = path_to_change->find(path);
  if (found != path_to_change->end()) {
    if (found->second.changestamp >= changestamp)
      return false;
    pending_changes_.erase(found->second.position_in_queue);
  }

  fileapi::FileSystemURL url(
      fileapi::CreateSyncableFileSystemURL(origin, kServiceName, path));

  fileapi::FileChange::ChangeType change_type;
  fileapi::SyncFileType file_type;
  if (is_deleted) {
    change_type = fileapi::FileChange::FILE_CHANGE_DELETE;
    file_type = fileapi::SYNC_FILE_TYPE_UNKNOWN;
  } else {
    change_type = fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE;
    file_type = fileapi::SYNC_FILE_TYPE_FILE;
  }

  fileapi::FileChange file_change(change_type, file_type);

  std::pair<PendingChangeQueue::iterator, bool> inserted_to_queue =
      pending_changes_.insert(ChangeQueueItem(changestamp, sync_type, url));
  DCHECK(inserted_to_queue.second);

  DVLOG(3) << "Append remote change: " << path.value()
           << "@" << changestamp << " "
           << file_change.DebugString();

  (*path_to_change)[path] = RemoteChange(
      changestamp, resource_id, sync_type, url, file_change,
      inserted_to_queue.first);
  return true;
}

void DriveFileSyncService::RemoveRemoteChange(
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

  if (metadata_store_->IsBatchSyncOrigin(url.origin()) &&
      !ContainsKey(url_to_change_, url.origin())) {
    metadata_store_->MoveBatchSyncOriginToIncremental(url.origin());
  }
}

bool DriveFileSyncService::GetPendingChangeForFileSystemURL(
    const fileapi::FileSystemURL& url,
    RemoteChange* change) const {
  DCHECK(change);
  URLToChange::const_iterator found_url = url_to_change_.find(url.origin());
  if (found_url == url_to_change_.end())
    return false;
  const PathToChange& path_to_change = found_url->second;
  PathToChange::const_iterator found_path = path_to_change.find(url.path());
  if (found_path == path_to_change.end())
    return false;
  *change = found_path->second;
  return true;
}

void DriveFileSyncService::FetchChangesForIncrementalSync() {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE, TASK_TYPE_DRIVE,
                                       "Fetching remote change list"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::FetchChangesForIncrementalSync, AsWeakPtr()));
    return;
  }

  polling_enabled_ = false;
  token->set_completion_callback(base::Bind(&EnablePolling, &polling_enabled_));

  if (metadata_store_->incremental_sync_origins().empty()) {
    NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
    return;
  }

  DVLOG(1) << "FetchChangesForIncrementalSync (start_changestamp:"
           << (largest_fetched_changestamp_ + 1) << ")";

  sync_client_->ListChanges(
      largest_fetched_changestamp_ + 1,
      base::Bind(&DriveFileSyncService::DidFetchChangesForIncrementalSync,
                 AsWeakPtr(), base::Passed(&token), false));
}

void DriveFileSyncService::DidFetchChangesForIncrementalSync(
    scoped_ptr<TaskToken> token,
    bool has_new_changes,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentFeed> changes) {
  if (error != google_apis::HTTP_SUCCESS) {
    NotifyTaskDone(GDataErrorCodeToSyncStatusCodeWrapper(error), token.Pass());
    return;
  }

  typedef ScopedVector<google_apis::DocumentEntry>::const_iterator iterator;
  for (iterator itr = changes->entries().begin();
       itr != changes->entries().end(); ++itr) {
    const google_apis::DocumentEntry& entry = **itr;
    GURL origin;
    if (!GetOriginForEntry(entry, &origin))
      continue;

    DVLOG(3) << " * change:" << entry.title()
             << (entry.deleted() ? " (deleted)" : " ")
             << "[" << origin.spec() << "]";
    has_new_changes =
        AppendRemoteChange(origin, entry, entry.changestamp(),
                           REMOTE_SYNC_TYPE_INCREMENTAL) || has_new_changes;
  }

  GURL next_feed;
  if (changes->GetNextFeedURL(&next_feed)) {
    sync_client_->ContinueListing(
        next_feed,
        base::Bind(&DriveFileSyncService::DidFetchChangesForIncrementalSync,
                   AsWeakPtr(), base::Passed(&token), has_new_changes));
    return;
  }

  largest_fetched_changestamp_ = changes->largest_changestamp();

  if (has_new_changes) {
    polling_delay_seconds_ = kMinimumPollingDelaySeconds;
  } else {
    // If the change_queue_ was not updated, update the polling delay to wait
    // longer.
    polling_delay_seconds_ = std::min(
        static_cast<int64>(kDelayMultiplier * polling_delay_seconds_),
        kMaximumPollingDelaySeconds);
  }

  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
}

bool DriveFileSyncService::GetOriginForEntry(
    const google_apis::DocumentEntry& entry,
    GURL* origin_out) {
  typedef ScopedVector<google_apis::Link>::const_iterator iterator;
  for (iterator itr = entry.links().begin();
       itr != entry.links().end(); ++itr) {
    if ((*itr)->type() != google_apis::Link::LINK_PARENT)
      continue;
    GURL origin(DriveFileSyncClient::DirectoryTitleToOrigin(
        UTF16ToUTF8((*itr)->title())));
    DCHECK(origin.is_valid());

    if (!metadata_store_->IsBatchSyncOrigin(origin) &&
        !metadata_store_->IsIncrementalSyncOrigin(origin))
      continue;
    std::string resource_id(metadata_store_->GetResourceIdForOrigin(origin));
    GURL resource_link(sync_client_->ResourceIdToResourceLink(resource_id));
    if ((*itr)->href().GetOrigin() != resource_link.GetOrigin() ||
        (*itr)->href().path() != resource_link.path())
      continue;

    *origin_out = origin;
    return true;
  }
  return false;
}

void DriveFileSyncService::SchedulePolling() {
  if (!pending_batch_sync_origins_.empty() ||
      metadata_store_->incremental_sync_origins().empty() ||
      !pending_changes_.empty() ||
      !polling_enabled_ ||
      polling_timer_.IsRunning())
    return;

  if (state_ != REMOTE_SERVICE_OK &&
      state_ != REMOTE_SERVICE_TEMPORARY_UNAVAILABLE)
    return;

  if (state_ == REMOTE_SERVICE_TEMPORARY_UNAVAILABLE)
    polling_delay_seconds_ = kMaximumPollingDelaySeconds;

  DVLOG(1) << "Polling scheduled"
           << " (delay:" << polling_delay_seconds_ << "s)";

  polling_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(polling_delay_seconds_),
      base::Bind(&DriveFileSyncService::FetchChangesForIncrementalSync,
                 AsWeakPtr()));
}

fileapi::SyncStatusCode
DriveFileSyncService::GDataErrorCodeToSyncStatusCodeWrapper(
    google_apis::GDataErrorCode error) const {
  fileapi::SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != fileapi::SYNC_STATUS_OK && !sync_client_->IsAuthenticated())
    return fileapi::SYNC_STATUS_AUTHENTICATION_FAILED;
  return status;
}

}  // namespace sync_file_system

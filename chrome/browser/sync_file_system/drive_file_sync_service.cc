// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "google/cacheinvalidation/types.pb.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_file_type.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

const base::FilePath::CharType kTempDirName[] = FILE_PATH_LITERAL("tmp");
const base::FilePath::CharType kSyncFileSystemDir[] =
    FILE_PATH_LITERAL("Sync FileSystem");

// The sync invalidation object ID for Google Drive.
const char kDriveInvalidationObjectId[] = "CHANGELOG";

// Incremental sync polling interval.
// TODO(calvinlo): Improve polling algorithm dependent on whether push
// notifications are on or off.
const int64 kMinimumPollingDelaySeconds = 5;
const int64 kMaximumPollingDelaySeconds = 10 * 60;  // 10 min
const int64 kPollingDelaySecondsWithNotification = 4 * 60 * 60;  // 4 hr
const double kDelayMultiplier = 1.6;

bool CreateTemporaryFile(const base::FilePath& dir_path,
                         base::FilePath* temp_file) {
  return file_util::CreateDirectory(dir_path) &&
      file_util::CreateTemporaryFileInDir(dir_path, temp_file);
}

void DeleteTemporaryFile(const base::FilePath& file_path) {
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

void EmptyStatusCallback(SyncStatusCode status) {}

void DidHandleUnregisteredOrigin(const GURL& origin, SyncStatusCode status) {
  // TODO(calvinlo): Disable syncing if status not ok (http://crbug.com/171611).
  DCHECK_EQ(SYNC_STATUS_OK, status);
  if (status != SYNC_STATUS_OK) {
    LOG(WARNING) << "Remove origin failed for: " << origin.spec()
                 << " status=" << status;
  }
}

FileChange CreateFileChange(bool is_deleted) {
  if (is_deleted)
    return FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_UNKNOWN);
  return FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE);
}

}  // namespace

const char DriveFileSyncService::kServiceName[] = "syncfs";
ConflictResolutionPolicy DriveFileSyncService::kDefaultPolicy =
    CONFLICT_RESOLUTION_LAST_WRITE_WIN;

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

    DVLOG(2) << "Token updated: " << description_
             << " " << location_.ToString();
  }

  const tracked_objects::Location& location() const { return location_; }
  TaskType task_type() const { return task_type_; }
  const std::string& description() const { return description_; }
  std::string done_description() const { return description_ + " done"; }

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

  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

void DriveFileSyncService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  SetPushNotificationEnabled(state);
}

void DriveFileSyncService::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  DCHECK_EQ(1U, invalidation_map.size());
  const invalidation::ObjectId object_id(
      ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
      kDriveInvalidationObjectId);
  DCHECK_EQ(1U, invalidation_map.count(object_id));

  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

struct DriveFileSyncService::ProcessRemoteChangeParam {
  scoped_ptr<TaskToken> token;
  RemoteChange remote_change;
  SyncFileCallback callback;

  DriveMetadata drive_metadata;
  SyncFileMetadata local_metadata;
  bool metadata_updated;
  base::FilePath temporary_file_path;
  std::string md5_checksum;
  SyncAction sync_action;
  bool clear_local_changes;

  ProcessRemoteChangeParam(scoped_ptr<TaskToken> token,
                           const RemoteChange& remote_change,
                           const SyncFileCallback& callback)
      : token(token.Pass()),
        remote_change(remote_change),
        callback(callback),
        metadata_updated(false),
        sync_action(SYNC_ACTION_NONE),
        clear_local_changes(true) {
  }
};

struct DriveFileSyncService::ApplyLocalChangeParam {
  scoped_ptr<TaskToken> token;
  FileSystemURL url;
  FileChange local_change;
  base::FilePath local_path;
  SyncFileMetadata local_metadata;
  DriveMetadata drive_metadata;
  bool has_drive_metadata;
  SyncStatusCallback callback;

  ApplyLocalChangeParam(scoped_ptr<TaskToken> token,
                        const FileSystemURL& url,
                        const FileChange& local_change,
                        const base::FilePath& local_path,
                        const SyncFileMetadata& local_metadata,
                        const SyncStatusCallback& callback)
      : token(token.Pass()),
        url(url),
        local_change(local_change),
        local_path(local_path),
        local_metadata(local_metadata),
        has_drive_metadata(false),
        callback(callback) {}
};

void DriveFileSyncService::SetRemoteChangeProcessor(
    RemoteChangeProcessor* processor) {
  remote_change_processor_ = processor;
}

DriveFileSyncService::ChangeQueueItem::ChangeQueueItem()
    : changestamp(0),
      sync_type(REMOTE_SYNC_TYPE_INCREMENTAL) {
}

DriveFileSyncService::ChangeQueueItem::ChangeQueueItem(
    int64 changestamp,
    RemoteSyncType sync_type,
    const FileSystemURL& url)
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
      change(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_UNKNOWN) {
}

DriveFileSyncService::RemoteChange::RemoteChange(
    int64 changestamp,
    const std::string& resource_id,
    const std::string& md5_checksum,
    const base::Time& updated_time,
    RemoteSyncType sync_type,
    const FileSystemURL& url,
    const FileChange& change,
    PendingChangeQueue::iterator position_in_queue)
    : changestamp(changestamp),
      resource_id(resource_id),
      md5_checksum(md5_checksum),
      updated_time(updated_time),
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
    : profile_(profile),
      last_operation_status_(SYNC_STATUS_OK),
      state_(REMOTE_SERVICE_OK),
      sync_enabled_(true),
      largest_fetched_changestamp_(0),
      push_notification_registered_(false),
      push_notification_enabled_(false),
      polling_delay_seconds_(kMinimumPollingDelaySeconds),
      may_have_unfetched_changes_(true),
      remote_change_processor_(NULL),
      conflict_resolution_(kDefaultPolicy),
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
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore,
                 AsWeakPtr(),
                 base::Passed(GetToken(FROM_HERE, TASK_TYPE_DATABASE,
                                       "Metadata database initialization"))));
}

DriveFileSyncService::~DriveFileSyncService() {
  // Invalidate WeakPtr instances here explicitly to notify TaskToken that we
  // can safely discard the token.
  weak_factory_.InvalidateWeakPtrs();
  if (sync_client_)
    sync_client_->RemoveObserver(this);
  token_.reset();

  // Unregister for Drive notifications.
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (!profile_sync_service || !push_notification_registered_) {
    return;
  }

  // TODO(calvinlo): Revisit this later in Consolidate Drive XMPP Notification
  // and Polling Backup into one Class patch. http://crbug/173339.
  // Original comment from Kochi about the order this is done in:
  // Once DriveSystemService gets started / stopped at runtime, this ID needs to
  // be unregistered *before* the handler is unregistered
  // as ID persists across browser restarts.
  profile_sync_service->UpdateRegisteredInvalidationIds(
      this, syncer::ObjectIdSet());
  profile_sync_service->UnregisterInvalidationHandler(this);
}

// static
scoped_ptr<DriveFileSyncService> DriveFileSyncService::CreateForTesting(
    Profile* profile,
    const base::FilePath& base_dir,
    scoped_ptr<DriveFileSyncClientInterface> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store) {
  return make_scoped_ptr(new DriveFileSyncService(
      profile, base_dir, sync_client.Pass(), metadata_store.Pass()));
}

scoped_ptr<DriveFileSyncClientInterface>
DriveFileSyncService::DestroyAndPassSyncClientForTesting(
    scoped_ptr<DriveFileSyncService> sync_service) {
  return sync_service->sync_client_.Pass();
}

void DriveFileSyncService::AddServiceObserver(Observer* observer) {
  service_observers_.AddObserver(observer);
}

void DriveFileSyncService::AddFileStatusObserver(
    FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void DriveFileSyncService::RegisterOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));
  scoped_ptr<TaskToken> token(GetToken(
          FROM_HERE, TASK_TYPE_DRIVE, "Retrieving origin metadata"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::RegisterOriginForTrackingChanges,
        AsWeakPtr(), origin, callback));
    return;
  }

  // We check state_ but not GetCurrentState() here as we want to accept the
  // registration if the server is available but sync_enabled_==false case.
  if (state_ == REMOTE_SERVICE_DISABLED) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(last_operation_status_, token.Pass());
    callback.Run(last_operation_status_);
    return;
  }

  DCHECK(!metadata_store_->IsOriginDisabled(origin));
  if (!metadata_store_->GetResourceIdForOrigin(origin).empty()) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(SYNC_STATUS_OK, token.Pass());
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  EnsureOriginRootDirectory(
      origin, base::Bind(&DriveFileSyncService::DidGetDriveDirectoryForOrigin,
                         AsWeakPtr(), base::Passed(&token), origin, callback));
}

void DriveFileSyncService::UnregisterOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE, TASK_TYPE_DATABASE, ""));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::UnregisterOriginForTrackingChanges,
        AsWeakPtr(), origin, callback));
    return;
  }

  // TODO(nhiroki): Add helper function to forget remote changes for the given
  // origin like "ForgetRemoteChangesForOrigin(origin)".
  // http://crbug.com/211600
  OriginToChangesMap::iterator found = origin_to_changes_map_.find(origin);
  if (found != origin_to_changes_map_.end()) {
    for (PathToChangeMap::iterator itr = found->second.begin();
         itr != found->second.end(); ++itr)
      pending_changes_.erase(itr->second.position_in_queue);
    origin_to_changes_map_.erase(found);
  }
  pending_batch_sync_origins_.erase(origin);

  metadata_store_->RemoveOrigin(origin, base::Bind(
      &DriveFileSyncService::DidChangeOriginOnMetadataStore,
      AsWeakPtr(), base::Passed(&token), callback));
}

void DriveFileSyncService::EnableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  if (!metadata_store_->IsOriginDisabled(origin))
    return;

  scoped_ptr<TaskToken> token(GetToken(FROM_HERE, TASK_TYPE_DATABASE, ""));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::EnableOriginForTrackingChanges,
        AsWeakPtr(), origin, callback));
    return;
  }

  metadata_store_->EnableOrigin(origin, base::Bind(
      &DriveFileSyncService::DidChangeOriginOnMetadataStore,
      AsWeakPtr(), base::Passed(&token), callback));
  pending_batch_sync_origins_.insert(origin);
}

void DriveFileSyncService::DisableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  if (!metadata_store_->IsBatchSyncOrigin(origin) &&
      !metadata_store_->IsIncrementalSyncOrigin(origin))
    return;

  scoped_ptr<TaskToken> token(GetToken(FROM_HERE, TASK_TYPE_DATABASE, ""));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::DisableOriginForTrackingChanges,
        AsWeakPtr(), origin, callback));
    return;
  }

  // TODO(nhiroki): Add helper function to forget remote changes for the given
  // origin like "ForgetRemoteChangesForOrigin(origin)".
  // http://crbug.com/211600
  OriginToChangesMap::iterator found = origin_to_changes_map_.find(origin);
  if (found != origin_to_changes_map_.end()) {
    for (PathToChangeMap::iterator itr = found->second.begin();
         itr != found->second.end(); ++itr)
      pending_changes_.erase(itr->second.position_in_queue);
    origin_to_changes_map_.erase(found);
  }
  pending_batch_sync_origins_.erase(origin);

  metadata_store_->DisableOrigin(origin, base::Bind(
      &DriveFileSyncService::DidChangeOriginOnMetadataStore,
      AsWeakPtr(), base::Passed(&token), callback));
}

void DriveFileSyncService::UninstallOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(
      FROM_HERE, TASK_TYPE_DATABASE, "Uninstall Origin"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::UninstallOrigin,
        AsWeakPtr(), origin, callback));
    return;
  }

  std::string resource_id = metadata_store_->GetResourceIdForOrigin(origin);

  // An empty resource_id indicates either one of following two cases:
  // 1) origin is not in metadata_store_ because the extension was never
  //    run and thus no origin directory on the remote drive was created.
  // 2) origin or sync root folder is deleted on Drive.
  if (resource_id.empty()) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  // Convert origin's directory GURL to ResourceID and delete it. Expected MD5
  // is empty to force delete (i.e. skip conflict resolution).
  sync_client_->DeleteFile(
      resource_id,
      std::string(),
      base::Bind(&DriveFileSyncService::DidUninstallOrigin,
                 AsWeakPtr(), base::Passed(&token), origin, callback));
}

void DriveFileSyncService::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  scoped_ptr<TaskToken> token(
      GetToken(FROM_HERE, TASK_TYPE_DRIVE, "Process remote change"));
  DCHECK(remote_change_processor_);

  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::ProcessRemoteChange, AsWeakPtr(), callback));
    return;
  }

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(last_operation_status_, token.Pass());
    callback.Run(last_operation_status_, FileSystemURL());
    return;
  }

  if (pending_changes_.empty()) {
    token->ResetTask(FROM_HERE);
    NotifyTaskDone(SYNC_STATUS_OK, token.Pass());
    callback.Run(SYNC_STATUS_NO_CHANGE_TO_SYNC, FileSystemURL());
    return;
  }

  const FileSystemURL& url = pending_changes_.begin()->url;
  const GURL& origin = url.origin();
  const base::FilePath& path = url.path();
  DCHECK(ContainsKey(origin_to_changes_map_, origin));
  PathToChangeMap* path_to_change = &origin_to_changes_map_[origin];
  DCHECK(ContainsKey(*path_to_change, path));
  const RemoteChange& remote_change = (*path_to_change)[path];

  DVLOG(1) << "ProcessRemoteChange for " << url.DebugString()
           << " remote_change:" << remote_change.change.DebugString();

  scoped_ptr<ProcessRemoteChangeParam> param(new ProcessRemoteChangeParam(
      token.Pass(), remote_change, callback));
  remote_change_processor_->PrepareForProcessRemoteChange(
      remote_change.url,
      kServiceName,
      base::Bind(&DriveFileSyncService::DidPrepareForProcessRemoteChange,
                 AsWeakPtr(), base::Passed(&param)));
}

LocalChangeProcessor* DriveFileSyncService::GetLocalChangeProcessor() {
  return this;
}

bool DriveFileSyncService::IsConflicting(const FileSystemURL& url) {
  DriveMetadata metadata;
  const SyncStatusCode status = metadata_store_->ReadEntry(url, &metadata);
  if (status != SYNC_STATUS_OK) {
    DCHECK_EQ(SYNC_DATABASE_ERROR_NOT_FOUND, status);
    return false;
  }
  return metadata.conflicted();
}

void DriveFileSyncService::GetRemoteFileMetadata(
    const FileSystemURL& url,
    const SyncFileMetadataCallback& callback) {
  DriveMetadata metadata;
  const SyncStatusCode status = metadata_store_->ReadEntry(url, &metadata);
  if (status != SYNC_STATUS_OK)
    callback.Run(status, SyncFileMetadata());
  sync_client_->GetResourceEntry(
      metadata.resource_id(),
      base::Bind(&DriveFileSyncService::DidGetRemoteFileMetadata,
                 AsWeakPtr(), callback));
}

RemoteServiceState DriveFileSyncService::GetCurrentState() const {
  if (!sync_enabled_)
    return REMOTE_SERVICE_DISABLED;
  return state_;
}

const char* DriveFileSyncService::GetServiceName() const {
  return kServiceName;
}

void DriveFileSyncService::SetSyncEnabled(bool enabled) {
  if (sync_enabled_ == enabled)
    return;

  RemoteServiceState old_state = GetCurrentState();
  sync_enabled_ = enabled;
  if (old_state == GetCurrentState())
    return;

  if (!enabled)
    last_operation_status_ = SYNC_STATUS_SYNC_DISABLED;

  const char* status_message = enabled ? "Sync is enabled" : "Sync is disabled";
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), status_message));

  if (GetCurrentState() == REMOTE_SERVICE_OK) {
    UpdatePollingDelay(kMinimumPollingDelaySeconds);
    SchedulePolling();
  }
}

SyncStatusCode DriveFileSyncService::SetConflictResolutionPolicy(
    ConflictResolutionPolicy resolution) {
  conflict_resolution_ = resolution;
  return SYNC_STATUS_OK;
}

ConflictResolutionPolicy
DriveFileSyncService::GetConflictResolutionPolicy() const {
  return conflict_resolution_;
}

void DriveFileSyncService::ApplyLocalChange(
    const FileChange& local_file_change,
    const base::FilePath& local_file_path,
    const SyncFileMetadata& local_file_metadata,
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  // TODO(nhiroki): support directory operations (http://crbug.com/161442).
  DCHECK(!local_file_change.IsDirectory());

  scoped_ptr<TaskToken> token(GetToken(
      FROM_HERE, TASK_TYPE_DATABASE, "Apply local change"));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::ApplyLocalChange, AsWeakPtr(),
        local_file_change, local_file_path, local_file_metadata,
        url, callback));
    return;
  }

  if (!metadata_store_->IsIncrementalSyncOrigin(url.origin()) &&
      !metadata_store_->IsBatchSyncOrigin(url.origin())) {
    // We may get called by LocalFileSyncService to sync local changes
    // for the origins that are disabled.
    DVLOG(1) << "Got request for stray origin: " << url.origin().spec();
    token->ResetTask(FROM_HERE);
    FinalizeLocalSync(token.Pass(), callback, SYNC_STATUS_NOT_INITIALIZED);
    return;
  }

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED) {
    token->ResetTask(FROM_HERE);
    FinalizeLocalSync(token.Pass(), callback, last_operation_status_);
    return;
  }

  scoped_ptr<ApplyLocalChangeParam> param(new ApplyLocalChangeParam(
      token.Pass(), url,
      local_file_change, local_file_path, local_file_metadata, callback));
  LocalSyncOperationType operation =
      ResolveLocalSyncOperationType(local_file_change, url, param.get());

  EnsureOriginRootDirectory(
      url.origin(),
      base::Bind(&DriveFileSyncService::ApplyLocalChangeInternal,
                 AsWeakPtr(), base::Passed(&param), operation));
}

void DriveFileSyncService::OnAuthenticated() {
  if (state_ == REMOTE_SERVICE_OK)
    return;
  DVLOG(1) << "OnAuthenticated";
  state_ = REMOTE_SERVICE_OK;
  if (GetCurrentState() != REMOTE_SERVICE_OK)
    return;
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), "Authenticated"));
  UpdatePollingDelay(kMinimumPollingDelaySeconds);

  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

void DriveFileSyncService::OnNetworkConnected() {
  if (state_ == REMOTE_SERVICE_OK)
    return;
  DVLOG(1) << "OnNetworkConnected";
  state_ = REMOTE_SERVICE_OK;
  if (GetCurrentState() != REMOTE_SERVICE_OK)
    return;
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), "Network connected"));
  UpdatePollingDelay(kMinimumPollingDelaySeconds);

  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

// Called by CreateForTesting.
DriveFileSyncService::DriveFileSyncService(
    Profile* profile,
    const base::FilePath& base_dir,
    scoped_ptr<DriveFileSyncClientInterface> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store)
    : profile_(profile),
      last_operation_status_(SYNC_STATUS_OK),
      state_(REMOTE_SERVICE_OK),
      sync_enabled_(true),
      largest_fetched_changestamp_(0),
      push_notification_registered_(false),
      push_notification_enabled_(false),
      polling_delay_seconds_(-1),
      may_have_unfetched_changes_(false),
      remote_change_processor_(NULL),
      conflict_resolution_(kDefaultPolicy),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(profile);
  temporary_file_dir_ = base_dir.Append(kTempDirName);

  token_.reset(new TaskToken(AsWeakPtr()));
  sync_client_ = sync_client.Pass();
  metadata_store_ = metadata_store.Pass();

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore,
                 AsWeakPtr(),
                 base::Passed(GetToken(FROM_HERE, TASK_TYPE_NONE,
                                       "Drive initialization for testing")),
                 SYNC_STATUS_OK, false));
}

scoped_ptr<DriveFileSyncService::TaskToken> DriveFileSyncService::GetToken(
    const tracked_objects::Location& from_here,
    TaskType task_type,
    const std::string& description) {
  if (!token_)
    return scoped_ptr<TaskToken>();
  TRACE_EVENT_ASYNC_BEGIN1("Sync FileSystem", "GetToken", this,
                           "description", description);
  token_->UpdateTask(from_here, task_type, description);
  return token_.Pass();
}

void DriveFileSyncService::NotifyTaskDone(SyncStatusCode status,
                                          scoped_ptr<TaskToken> token) {
  DCHECK(token);
  last_operation_status_ = status;
  token_ = token.Pass();
  TRACE_EVENT_ASYNC_END0("Sync FileSystem", "GetToken", this);
  if (status != SYNC_STATUS_OK)
    DVLOG(2) << "DriveFileSyncService Task failed: " << status;

  if (token_->task_type() != TASK_TYPE_NONE) {
    DVLOG(2) << "NotifyTaskDone: " << token_->description()
             << ": finished with status=" << status
             << " (" << SyncStatusCodeToString(status) << ")"
             << " " << token_->location().ToString();

    RemoteServiceState old_state = GetCurrentState();
    UpdateServiceState();

    // Reset the polling delay. This will adjust the polling timer
    // based on the current service state.
    UpdatePollingDelay(polling_delay_seconds_);

    // Notify remote sync service state if the state has been changed.
    if (!token_->description().empty() || old_state != GetCurrentState()) {
      FOR_EACH_OBSERVER(
          Observer, service_observers_,
          OnRemoteServiceStateUpdated(GetCurrentState(),
                                      token_->done_description()));
    }
  }

  token_->ResetTask(FROM_HERE);
  if (!pending_tasks_.empty()) {
    base::Closure closure = pending_tasks_.front();
    pending_tasks_.pop_front();
    closure.Run();
    return;
  }

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  MaybeStartFetchChanges();

  SchedulePolling();

  // Notify observer of the update of |pending_changes_|.
  FOR_EACH_OBSERVER(Observer, service_observers_,
                    OnRemoteChangeQueueUpdated(pending_changes_.size()));
}

void DriveFileSyncService::UpdateServiceState() {
  switch (last_operation_status_) {
    // Possible regular operation errors.
    case SYNC_STATUS_OK:
    case SYNC_STATUS_FILE_BUSY:
    case SYNC_STATUS_HAS_CONFLICT:
    case SYNC_STATUS_NO_CONFLICT:
    case SYNC_STATUS_NO_CHANGE_TO_SYNC:
    case SYNC_FILE_ERROR_NOT_FOUND:
    case SYNC_FILE_ERROR_FAILED:
    case SYNC_FILE_ERROR_NO_SPACE:
      // If the service type was DRIVE and the status was ok, the state
      // should be migrated to OK state.
      if (token_->task_type() == TASK_TYPE_DRIVE)
        state_ = REMOTE_SERVICE_OK;
      break;

    // Authentication error.
    case SYNC_STATUS_AUTHENTICATION_FAILED:
      state_ = REMOTE_SERVICE_AUTHENTICATION_REQUIRED;
      break;

    // Errors which could make the service temporarily unavailable.
    case SYNC_STATUS_RETRY:
    case SYNC_STATUS_NETWORK_ERROR:
    case SYNC_STATUS_ABORT:
    case SYNC_STATUS_FAILED:
      state_ = REMOTE_SERVICE_TEMPORARY_UNAVAILABLE;
      break;

    // Errors which would require manual user intervention to resolve.
    case SYNC_DATABASE_ERROR_CORRUPTION:
    case SYNC_DATABASE_ERROR_IO_ERROR:
    case SYNC_DATABASE_ERROR_FAILED:
      state_ = REMOTE_SERVICE_DISABLED;
      break;

    // Requested origin is not initialized or has been unregistered.
    // This shouldn't affect the global service state.
    case SYNC_STATUS_NOT_INITIALIZED:
      break;

    // Unexpected status code. They should be explicitly added to one of the
    // above three cases.
    default:
      LOG(WARNING) << "Unexpected status returned: " << last_operation_status_;
      state_ = REMOTE_SERVICE_TEMPORARY_UNAVAILABLE;
      NOTREACHED();
      break;
  }
}

base::WeakPtr<DriveFileSyncService> DriveFileSyncService::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DriveFileSyncService::DidInitializeMetadataStore(
    scoped_ptr<TaskToken> token,
    SyncStatusCode status,
    bool created) {
  if (status != SYNC_STATUS_OK) {
    NotifyTaskDone(status, token.Pass());
    return;
  }

  UpdateRegisteredOrigins();

  largest_fetched_changestamp_ = metadata_store_->GetLargestChangeStamp();

  // Mark all the batch sync origins as 'pending' so that we can start
  // batch sync when we're ready.
  DCHECK(pending_batch_sync_origins_.empty());
  for (std::map<GURL, std::string>::const_iterator itr =
          metadata_store_->batch_sync_origins().begin();
        itr != metadata_store_->batch_sync_origins().end();
        ++itr) {
    pending_batch_sync_origins_.insert(itr->first);
  }

  DriveMetadataStore::URLAndResourceIdList to_be_fetched_files;
  status = metadata_store_->GetToBeFetchedFiles(&to_be_fetched_files);
  DCHECK_EQ(SYNC_STATUS_OK, status);
  typedef DriveMetadataStore::URLAndResourceIdList::const_iterator iterator;
  for (iterator itr = to_be_fetched_files.begin();
       itr != to_be_fetched_files.end(); ++itr) {
    DriveMetadata metadata;
    const FileSystemURL& url = itr->first;
    const std::string& resource_id = itr->second;
    AppendFetchChange(url.origin(), url.path(), resource_id);
  }

  if (!sync_root_resource_id().empty())
    sync_client_->EnsureSyncRootIsNotInMyDrive(sync_root_resource_id());

  NotifyTaskDone(status, token.Pass());
  RegisterDriveNotifications();
}

void DriveFileSyncService::UpdateRegisteredOrigins() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!extension_service)
    return;

  // TODO(nhiroki): clean up these loops with similar bodies.
  // http://crbug.com/211600

  std::vector<GURL> disabled_origins;
  metadata_store_->GetDisabledOrigins(&disabled_origins);
  for (std::vector<GURL>::const_iterator itr = disabled_origins.begin();
       itr != disabled_origins.end(); ++itr) {
    std::string extension_id = itr->host();
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(extension_id);

    if (!extension_service->GetInstalledExtension(extension_id)) {
      UninstallOrigin(origin, base::Bind(&DidHandleUnregisteredOrigin, origin));
      continue;
    }

    if (extension_service->IsExtensionEnabled(extension_id)) {
      // Extension is enabled. Enable origin.
      metadata_store_->EnableOrigin(
          origin, base::Bind(&EmptyStatusCallback));
      pending_batch_sync_origins_.insert(origin);
      continue;
    }

    // Extension is still disabled.
  }

  std::vector<GURL> enabled_origins;
  metadata_store_->GetEnabledOrigins(&enabled_origins);
  for (std::vector<GURL>::const_iterator itr = enabled_origins.begin();
       itr != enabled_origins.end(); ++itr) {
    std::string extension_id = itr->host();
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(extension_id);

    if (!extension_service->GetInstalledExtension(extension_id)) {
      UninstallOrigin(origin, base::Bind(&DidHandleUnregisteredOrigin, origin));
      continue;
    }

    if (!extension_service->IsExtensionEnabled(extension_id)) {
      // Extension is disabled. Disable origin.
      metadata_store_->DisableOrigin(
          origin, base::Bind(&EmptyStatusCallback));
      continue;
    }

    // Extension is still enabled.
  }
}

void DriveFileSyncService::StartBatchSyncForOrigin(
    const GURL& origin,
    const std::string& resource_id) {
  scoped_ptr<TaskToken> token(
      GetToken(FROM_HERE, TASK_TYPE_DRIVE, "Retrieving largest changestamp"));
  DCHECK(token);
  DCHECK(GetCurrentState() == REMOTE_SERVICE_OK || may_have_unfetched_changes_);
  DCHECK(!metadata_store_->IsOriginDisabled(origin));

  DVLOG(1) << "Start batch sync for:" << origin.spec();

  sync_client_->GetLargestChangeStamp(
      base::Bind(&DriveFileSyncService::DidGetLargestChangeStampForBatchSync,
                 AsWeakPtr(), base::Passed(&token), origin, resource_id));

  may_have_unfetched_changes_ = false;
}

void DriveFileSyncService::DidGetDriveDirectoryForOrigin(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    const std::string& resource_id) {
  if (status != SYNC_STATUS_OK) {
    NotifyTaskDone(status, token.Pass());
    callback.Run(status);
    return;
  }

  // Add this origin to batch sync origin if it hasn't been already.
  if (!metadata_store_->IsKnownOrigin(origin)) {
    metadata_store_->AddBatchSyncOrigin(origin, resource_id);
    pending_batch_sync_origins_.insert(origin);
  }

  NotifyTaskDone(SYNC_STATUS_OK, token.Pass());
  callback.Run(SYNC_STATUS_OK);
}

void DriveFileSyncService::DidUninstallOrigin(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  NotifyTaskDone(status, token.Pass());

  // Origin directory has been removed so it's now safe to remove the origin
  // from the metadata store.
  UnregisterOriginForTrackingChanges(origin, callback);
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
    scoped_ptr<google_apis::ResourceList> feed) {
  if (error != google_apis::HTTP_SUCCESS) {
    pending_batch_sync_origins_.insert(origin);
    // TODO(tzik): Refine this error code.
    NotifyTaskDone(GDataErrorCodeToSyncStatusCodeWrapper(error), token.Pass());
    return;
  }

  typedef ScopedVector<google_apis::ResourceEntry>::const_iterator iterator;
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

  // Move |origin| to the incremental sync origin set if the origin has no file.
  if (metadata_store_->IsBatchSyncOrigin(origin) &&
      !ContainsKey(origin_to_changes_map_, origin)) {
    metadata_store_->MoveBatchSyncOriginToIncremental(origin);
  }

  // If this was the last batch sync origin and push_notification is enabled
  // (indicates that we may have longer polling cycle), trigger the first
  // incremental sync on next task cycle.
  if (pending_batch_sync_origins_.empty() &&
      push_notification_enabled_) {
    may_have_unfetched_changes_ = true;
  }

  NotifyTaskDone(SYNC_STATUS_OK, token.Pass());
}

void DriveFileSyncService::DidChangeOriginOnMetadataStore(
    scoped_ptr<TaskToken> token,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  NotifyTaskDone(status, token.Pass());
  callback.Run(status);
}

void DriveFileSyncService::DidGetRemoteFileMetadata(
    const SyncFileMetadataCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error),
                 SyncFileMetadata());
    return;
  }

  DCHECK(entry);
  SyncFileType file_type = SYNC_FILE_TYPE_UNKNOWN;
  if (entry->is_file())
    file_type = SYNC_FILE_TYPE_FILE;
  else if (entry->is_folder())
    file_type = SYNC_FILE_TYPE_DIRECTORY;
  callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error),
               SyncFileMetadata(file_type,
                                entry->file_size(),
                                entry->updated_time()));
}

void DriveFileSyncService::ApplyLocalChangeInternal(
    scoped_ptr<ApplyLocalChangeParam> param,
    LocalSyncOperationType operation,
    SyncStatusCode status,
    const std::string& origin_resource_id) {
  if (status != SYNC_STATUS_OK) {
    FinalizeLocalSync(param->token.Pass(), param->callback, status);
    return;
  }

  const FileSystemURL& url = param->url;
  const FileChange& local_file_change = param->local_change;
  const base::FilePath& local_file_path = param->local_path;
  DriveMetadata& drive_metadata = param->drive_metadata;
  const SyncStatusCallback& callback = param->callback;

  DVLOG(1) << "ApplyLocalChange for " << url.DebugString()
           << " local_change:" << local_file_change.DebugString()
           << " ==> operation:" << operation;

  switch (operation) {
    case LOCAL_SYNC_OPERATION_ADD: {
      sync_client_->UploadNewFile(
          origin_resource_id,
          local_file_path,
          url.path().AsUTF8Unsafe(),
          base::Bind(&DriveFileSyncService::DidUploadNewFileForLocalSync,
                    AsWeakPtr(), base::Passed(&param)));
      return;
    }
    case LOCAL_SYNC_OPERATION_UPDATE: {
      DCHECK(param->has_drive_metadata);
      sync_client_->UploadExistingFile(
          drive_metadata.resource_id(),
          drive_metadata.md5_checksum(),
          local_file_path,
          base::Bind(&DriveFileSyncService::DidUploadExistingFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    }
    case LOCAL_SYNC_OPERATION_DELETE: {
      DCHECK(param->has_drive_metadata);
      sync_client_->DeleteFile(
          drive_metadata.resource_id(),
          drive_metadata.md5_checksum(),
          base::Bind(&DriveFileSyncService::DidDeleteFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    }
    case LOCAL_SYNC_OPERATION_NONE_CONFLICTED:
      // The file is already conflicted.
      HandleConflictForLocalSync(param.Pass());
      return;
    case LOCAL_SYNC_OPERATION_NONE:
      FinalizeLocalSync(param->token.Pass(), callback, SYNC_STATUS_OK);
      return;
    case LOCAL_SYNC_OPERATION_CONFLICT:
      HandleConflictForLocalSync(param.Pass());
      return;
    case LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE: {
      ResolveConflictToRemoteForLocalSync(param.Pass());
      return;
    }
    case LOCAL_SYNC_OPERATION_FAIL: {
      FinalizeLocalSync(param->token.Pass(), callback, SYNC_STATUS_FAILED);
      return;
    }
  }
  NOTREACHED();
  FinalizeLocalSync(param->token.Pass(), callback, SYNC_STATUS_FAILED);
}

DriveFileSyncService::LocalSyncOperationType
DriveFileSyncService::ResolveLocalSyncOperationType(
    const FileChange& local_file_change,
    const FileSystemURL& url,
    ApplyLocalChangeParam* param) {
  DriveMetadata metadata;
  const bool has_metadata =
      (metadata_store_->ReadEntry(url, &metadata) == SYNC_STATUS_OK);

  if (param) {
    param->has_drive_metadata = has_metadata;
    param->drive_metadata = metadata;
    if (!has_metadata)
      param->drive_metadata.set_md5_checksum(std::string());
  }

  if (has_metadata && metadata.conflicted()) {
    // The file has been marked as conflicted.
    if (local_file_change.IsAddOrUpdate())
      return LOCAL_SYNC_OPERATION_NONE_CONFLICTED;
    else if (local_file_change.IsDelete())
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    NOTREACHED();
    return LOCAL_SYNC_OPERATION_FAIL;
  }

  RemoteChange remote_change;
  const bool has_remote_change = GetPendingChangeForFileSystemURL(
      url, &remote_change);

  if (has_remote_change) {
    if (param && !has_metadata)
      param->drive_metadata.set_resource_id(remote_change.resource_id);

    // Remote change for the file identified by |url| exists in the pending
    // change queue.
    const FileChange& remote_file_change = remote_change.change;

    // (RemoteChange) + (LocalChange) -> (Operation Type)
    // AddOrUpdate    + AddOrUpdate   -> CONFLICT
    // AddOrUpdate    + Delete        -> NONE
    // Delete         + AddOrUpdate   -> ADD
    // Delete         + Delete        -> NONE

    if (remote_file_change.IsAddOrUpdate() && local_file_change.IsAddOrUpdate())
      return LOCAL_SYNC_OPERATION_CONFLICT;
    else if (remote_file_change.IsDelete() && local_file_change.IsAddOrUpdate())
      return LOCAL_SYNC_OPERATION_ADD;
    else if (local_file_change.IsDelete())
      return LOCAL_SYNC_OPERATION_NONE;
    NOTREACHED();
    return LOCAL_SYNC_OPERATION_FAIL;
  }

  if (has_metadata) {
    // The remote file identified by |url| exists on Drive.
    if (local_file_change.IsAddOrUpdate())
      return LOCAL_SYNC_OPERATION_UPDATE;
    else if (local_file_change.IsDelete())
      return LOCAL_SYNC_OPERATION_DELETE;
    NOTREACHED();
    return LOCAL_SYNC_OPERATION_FAIL;
  }

  // The remote file identified by |url| does not exist on Drive.
  if (local_file_change.IsAddOrUpdate())
    return LOCAL_SYNC_OPERATION_ADD;
  else if (local_file_change.IsDelete())
    return LOCAL_SYNC_OPERATION_NONE;
  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

void DriveFileSyncService::DidApplyLocalChange(
    scoped_ptr<ApplyLocalChangeParam> param,
    const google_apis::GDataErrorCode error,
    SyncStatusCode status) {
  if (status == SYNC_STATUS_OK) {
    RemoveRemoteChange(param->url);
    status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  }
  FinalizeLocalSync(param->token.Pass(), param->callback, status);
}

void DriveFileSyncService::DidResolveConflictToRemoteChange(
    scoped_ptr<ApplyLocalChangeParam> param,
    SyncStatusCode status) {
  DCHECK(param->has_drive_metadata);
  if (status != SYNC_STATUS_OK)
    FinalizeLocalSync(param->token.Pass(), param->callback, status);
  AppendFetchChange(param->url.origin(), param->url.path(),
                    param->drive_metadata.resource_id());
  FinalizeLocalSync(param->token.Pass(), param->callback, status);
}

void DriveFileSyncService::FinalizeLocalSync(
    scoped_ptr<TaskToken> token,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  NotifyTaskDone(status, token.Pass());
  callback.Run(status);
}

void DriveFileSyncService::DidUploadNewFileForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& file_md5) {
  DCHECK(param);
  DCHECK(!param->has_drive_metadata);
  const FileSystemURL& url = param->url;
  if (error == google_apis::HTTP_CREATED) {
    param->drive_metadata.set_resource_id(resource_id);
    param->drive_metadata.set_md5_checksum(file_md5);
    param->drive_metadata.set_conflicted(false);
    param->drive_metadata.set_to_be_fetched(false);
    const DriveMetadata& metadata = param->drive_metadata;
    metadata_store_->UpdateEntry(
        url, metadata,
        base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                   AsWeakPtr(), base::Passed(&param), error));
    NotifyObserversFileStatusChanged(url,
                                     SYNC_FILE_STATUS_SYNCED,
                                     SYNC_ACTION_ADDED,
                                     SYNC_DIRECTION_LOCAL_TO_REMOTE);
    return;
  }
  FinalizeLocalSync(param->token.Pass(), param->callback,
                    GDataErrorCodeToSyncStatusCodeWrapper(error));
}

void DriveFileSyncService::DidUploadExistingFileForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& file_md5) {
  DCHECK(param);
  DCHECK(param->has_drive_metadata);
  const FileSystemURL& url = param->url;
  switch (error) {
    case google_apis::HTTP_SUCCESS: {
      param->drive_metadata.set_resource_id(resource_id);
      param->drive_metadata.set_md5_checksum(file_md5);
      param->drive_metadata.set_conflicted(false);
      param->drive_metadata.set_to_be_fetched(false);
      const DriveMetadata& metadata = param->drive_metadata;
      metadata_store_->UpdateEntry(
          url, metadata,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&param), error));
      NotifyObserversFileStatusChanged(url,
                                       SYNC_FILE_STATUS_SYNCED,
                                       SYNC_ACTION_UPDATED,
                                       SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    }
    case google_apis::HTTP_CONFLICT: {
      HandleConflictForLocalSync(param.Pass());
      return;
    }
    case google_apis::HTTP_NOT_MODIFIED: {
      DidApplyLocalChange(param.Pass(),
                          google_apis::HTTP_SUCCESS, SYNC_STATUS_OK);
      return;
    }
    case google_apis::HTTP_NOT_FOUND: {
      const base::FilePath& local_file_path = param->local_path;
      sync_client_->UploadNewFile(
          metadata_store_->GetResourceIdForOrigin(url.origin()),
          local_file_path,
          url.path().AsUTF8Unsafe(),
          base::Bind(&DriveFileSyncService::DidUploadNewFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    }
    default: {
      const SyncStatusCode status =
          GDataErrorCodeToSyncStatusCodeWrapper(error);
      DCHECK_NE(SYNC_STATUS_OK, status);
      FinalizeLocalSync(param->token.Pass(), param->callback, status);
      return;
    }
  }
}

void DriveFileSyncService::DidDeleteFileForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    google_apis::GDataErrorCode error) {
  DCHECK(param);
  DCHECK(param->has_drive_metadata);
  const FileSystemURL& url = param->url;
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
      metadata_store_->DeleteEntry(
          url,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&param), error));
      NotifyObserversFileStatusChanged(url,
                                       SYNC_FILE_STATUS_SYNCED,
                                       SYNC_ACTION_DELETED,
                                       SYNC_DIRECTION_LOCAL_TO_REMOTE);
      return;
    case google_apis::HTTP_NOT_FOUND:
      DidApplyLocalChange(param.Pass(),
                          google_apis::HTTP_SUCCESS, SYNC_STATUS_OK);
      return;
    default: {
      const SyncStatusCode status =
          GDataErrorCodeToSyncStatusCodeWrapper(error);
      DCHECK_NE(SYNC_STATUS_OK, status);
      FinalizeLocalSync(param->token.Pass(), param->callback, status);
      return;
    }
  }
}

void DriveFileSyncService::HandleConflictForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param) {
  DCHECK(param);
  const FileSystemURL& url = param->url;
  DriveMetadata& drive_metadata = param->drive_metadata;
  if (conflict_resolution_ == CONFLICT_RESOLUTION_MANUAL) {
    if (drive_metadata.conflicted()) {
      // It's already conflicting; no need to update metadata.
      FinalizeLocalSync(param->token.Pass(),
                        param->callback, SYNC_STATUS_FAILED);
      return;
    }
    MarkConflict(url, &drive_metadata,
                 base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                            AsWeakPtr(), base::Passed(&param),
                            google_apis::HTTP_CONFLICT));
    return;
  }

  DCHECK_EQ(CONFLICT_RESOLUTION_LAST_WRITE_WIN, conflict_resolution_);

  DCHECK(!drive_metadata.resource_id().empty());
  sync_client_->GetResourceEntry(
      drive_metadata.resource_id(), base::Bind(
          &DriveFileSyncService::DidGetRemoteFileMetadataForRemoteUpdatedTime,
          AsWeakPtr(),
          base::Bind(&DriveFileSyncService::ResolveConflictForLocalSync,
                     AsWeakPtr(), base::Passed(&param))));
}

void DriveFileSyncService::ResolveConflictForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    const base::Time& remote_updated_time,
    SyncStatusCode status) {
  DCHECK(param);
  const FileSystemURL& url = param->url;
  DriveMetadata& drive_metadata = param->drive_metadata;
  SyncFileMetadata& local_metadata = param->local_metadata;
  if (status != SYNC_STATUS_OK) {
    FinalizeLocalSync(param->token.Pass(), param->callback, status);
    return;
  }

  if (local_metadata.last_modified >= remote_updated_time) {
    // Local win case.
    DVLOG(1) << "Resolving conflict for local sync:"
             << url.DebugString() << ": LOCAL WIN";
    if (!param->has_drive_metadata) {
      StartOverLocalSync(param.Pass(), SYNC_STATUS_OK);
      return;
    }
    // Make sure we reset the conflict flag and start over the local sync
    // with empty remote changes.
    DCHECK(!drive_metadata.resource_id().empty());
    drive_metadata.set_md5_checksum(std::string());
    drive_metadata.set_conflicted(false);
    drive_metadata.set_to_be_fetched(false);
    metadata_store_->UpdateEntry(
        url, drive_metadata,
        base::Bind(&DriveFileSyncService::StartOverLocalSync, AsWeakPtr(),
                   base::Passed(&param)));
    return;
  }
  // Remote win case.
  DVLOG(1) << "Resolving conflict for local sync:"
           << url.DebugString() << ": REMOTE WIN";
  ResolveConflictToRemoteForLocalSync(param.Pass());
}

void DriveFileSyncService::ResolveConflictToRemoteForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param) {
  DCHECK(param);
  const FileSystemURL& url = param->url;
  DriveMetadata& drive_metadata = param->drive_metadata;
  // Mark the file as to-be-fetched.
  DCHECK(!drive_metadata.resource_id().empty());
  drive_metadata.set_conflicted(false);
  drive_metadata.set_to_be_fetched(true);
  param->has_drive_metadata = true;
  metadata_store_->UpdateEntry(
      url, drive_metadata,
      base::Bind(&DriveFileSyncService::DidResolveConflictToRemoteChange,
                 AsWeakPtr(), base::Passed(&param)));
  // The synced notification will be dispatched when the remote file is
  // downloaded.
}

void DriveFileSyncService::StartOverLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    SyncStatusCode status) {
  DCHECK(param);
  if (status != SYNC_STATUS_OK) {
    FinalizeLocalSync(param->token.Pass(), param->callback, status);
    return;
  }
  RemoveRemoteChange(param->url);
  pending_tasks_.push_front(base::Bind(
      &DriveFileSyncService::ApplyLocalChange,
      AsWeakPtr(),
      param->local_change, param->local_path, param->local_metadata,
      param->url, param->callback));
  param->token->ResetTask(FROM_HERE);
  NotifyTaskDone(status, param->token.Pass());
}

void DriveFileSyncService::DidPrepareForProcessRemoteChange(
    scoped_ptr<ProcessRemoteChangeParam> param,
    SyncStatusCode status,
    const SyncFileMetadata& metadata,
    const FileChangeList& local_changes) {
  if (status != SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  param->local_metadata = metadata;
  const FileSystemURL& url = param->remote_change.url;
  const DriveMetadata& drive_metadata = param->drive_metadata;
  const FileChange& remote_file_change = param->remote_change.change;

  status = metadata_store_->ReadEntry(param->remote_change.url,
                                      &param->drive_metadata);
  DCHECK(status == SYNC_STATUS_OK || status == SYNC_DATABASE_ERROR_NOT_FOUND);

  bool missing_db_entry = (status != SYNC_STATUS_OK);
  if (missing_db_entry) {
    param->drive_metadata.set_resource_id(param->remote_change.resource_id);
    param->drive_metadata.set_md5_checksum(std::string());
    param->drive_metadata.set_conflicted(false);
    param->drive_metadata.set_to_be_fetched(false);
  }
  bool missing_local_file = (metadata.file_type == SYNC_FILE_TYPE_UNKNOWN);

  if (param->drive_metadata.conflicted()) {
    if (missing_local_file) {
      if (remote_file_change.IsAddOrUpdate()) {
        // Resolve conflict to remote change automatically.
        DVLOG(1) << "ProcessRemoteChange for " << url.DebugString()
                 << (param->drive_metadata.conflicted() ? " (conflicted)" : " ")
                 << (missing_local_file ? " (missing local file)" : " ")
                 << " remote_change: " << remote_file_change.DebugString()
                 << " ==> operation: ResolveConflictToRemoteChange";

        const FileSystemURL& url = param->remote_change.url;
        param->drive_metadata.set_conflicted(false);
        param->drive_metadata.set_to_be_fetched(true);
        metadata_store_->UpdateEntry(
            url, drive_metadata, base::Bind(&EmptyStatusCallback));

        param->sync_action = SYNC_ACTION_ADDED;
        DownloadForRemoteSync(param.Pass());
        return;
      }

      DCHECK(remote_file_change.IsDelete());
      param->sync_action = SYNC_ACTION_NONE;
      DeleteMetadataForRemoteSync(param.Pass());
      return;
    }

    DCHECK(!missing_local_file);
    if (remote_file_change.IsAddOrUpdate()) {
      HandleConflictForRemoteSync(param.Pass(), base::Time(), SYNC_STATUS_OK);
      return;
    }

    DCHECK(remote_file_change.IsDelete());
    DVLOG(1) << "ProcessRemoteChange for " << url.DebugString()
             << (param->drive_metadata.conflicted() ? " (conflicted)" : " ")
             << (missing_local_file ? " (missing local file)" : " ")
             << " remote_change: " << remote_file_change.DebugString()
             << " ==> operation: ResolveConflictToLocalChange";

    ResolveConflictToLocalForRemoteSync(param.Pass());
    return;
  }

  DCHECK(!param->drive_metadata.conflicted());
  if (remote_file_change.IsAddOrUpdate()) {
    if (local_changes.empty()) {
      if (missing_local_file) {
        param->sync_action = SYNC_ACTION_ADDED;
        DownloadForRemoteSync(param.Pass());
        return;
      }

      DCHECK(!missing_local_file);
      param->sync_action = SYNC_ACTION_UPDATED;
      DownloadForRemoteSync(param.Pass());
      return;
    }

    DCHECK(!local_changes.empty());
    if (local_changes.list().back().IsAddOrUpdate()) {
      HandleConflictForRemoteSync(param.Pass(), base::Time(), SYNC_STATUS_OK);
      return;
    }

    DCHECK(local_changes.list().back().IsDelete());
    param->sync_action = SYNC_ACTION_ADDED;
    DownloadForRemoteSync(param.Pass());
    return;
  }

  DCHECK(remote_file_change.IsDelete());
  if (local_changes.empty()) {
    if (missing_local_file) {
      param->sync_action = SYNC_ACTION_NONE;
      if (missing_db_entry)
        CompleteRemoteSync(param.Pass(), SYNC_STATUS_OK);
      else
        DeleteMetadataForRemoteSync(param.Pass());
      return;
    }
    DCHECK(!missing_local_file);
    param->sync_action = SYNC_ACTION_DELETED;

    const FileChange& file_change = remote_file_change;
    remote_change_processor_->ApplyRemoteChange(
        file_change, base::FilePath(), url,
        base::Bind(&DriveFileSyncService::DidApplyRemoteChange, AsWeakPtr(),
                   base::Passed(&param)));
    return;
  }

  DCHECK(!local_changes.empty());
  if (local_changes.list().back().IsAddOrUpdate()) {
    param->sync_action = SYNC_ACTION_NONE;
    CompleteRemoteSync(param.Pass(), SYNC_STATUS_OK);
    return;
  }

  DCHECK(local_changes.list().back().IsDelete());
  param->sync_action = SYNC_ACTION_NONE;
  if (missing_db_entry)
    CompleteRemoteSync(param.Pass(), SYNC_STATUS_OK);
  else
    DeleteMetadataForRemoteSync(param.Pass());
}

void DriveFileSyncService::DidResolveConflictToLocalChange(
    scoped_ptr<ProcessRemoteChangeParam> param,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    DCHECK_NE(SYNC_STATUS_HAS_CONFLICT, status);
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  const FileSystemURL& url = param->remote_change.url;
  if (param->remote_change.change.IsDelete()) {
    metadata_store_->DeleteEntry(
        url,
        base::Bind(&DriveFileSyncService::CompleteRemoteSync,
                   AsWeakPtr(), base::Passed(&param)));
  } else {
    DriveMetadata& drive_metadata = param->drive_metadata;
    DCHECK(!param->remote_change.resource_id.empty());
    drive_metadata.set_resource_id(param->remote_change.resource_id);
    drive_metadata.set_conflicted(false);
    drive_metadata.set_to_be_fetched(false);
    drive_metadata.set_md5_checksum(std::string());
    metadata_store_->UpdateEntry(
        url, drive_metadata,
        base::Bind(&DriveFileSyncService::CompleteRemoteSync,
                   AsWeakPtr(), base::Passed(&param)));
  }
}

void DriveFileSyncService::DownloadForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param) {
  // TODO(tzik): Use ShareableFileReference here after we get thread-safe
  // version of it. crbug.com/162598
  base::FilePath* temporary_file_path = &param->temporary_file_path;
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
    AbortRemoteSync(param.Pass(), SYNC_FILE_ERROR_FAILED);
    return;
  }

  const base::FilePath& temporary_file_path = param->temporary_file_path;
  std::string resource_id = param->remote_change.resource_id;

  // We should not use the md5 in metadata for FETCH type to avoid the download
  // finishes due to NOT_MODIFIED.
  std::string md5_checksum;
  if (param->remote_change.sync_type != REMOTE_SYNC_TYPE_FETCH)
    md5_checksum = param->drive_metadata.md5_checksum();

  sync_client_->DownloadFile(
      resource_id, md5_checksum,
      temporary_file_path,
      base::Bind(&DriveFileSyncService::DidDownloadFileForRemoteSync,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DidDownloadFileForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    google_apis::GDataErrorCode error,
    const std::string& md5_checksum) {
  if (error == google_apis::HTTP_NOT_MODIFIED) {
    param->sync_action = SYNC_ACTION_NONE;
    DidApplyRemoteChange(param.Pass(), SYNC_STATUS_OK);
    return;
  }

  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status != SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  param->drive_metadata.set_md5_checksum(md5_checksum);
  const FileChange& change = param->remote_change.change;
  const base::FilePath& temporary_file_path = param->temporary_file_path;
  const FileSystemURL& url = param->remote_change.url;
  remote_change_processor_->ApplyRemoteChange(
      change, temporary_file_path, url,
      base::Bind(&DriveFileSyncService::DidApplyRemoteChange,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DidApplyRemoteChange(
    scoped_ptr<ProcessRemoteChangeParam> param,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
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
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }

  RemoveRemoteChange(param->remote_change.url);

  if (param->drive_metadata.to_be_fetched()) {
    // Clear |to_be_fetched| flag since we completed fetching the remote change
    // and applying it to the local file.
    DCHECK(!param->drive_metadata.conflicted());
    param->drive_metadata.set_conflicted(false);
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
    status = SYNC_STATUS_HAS_CONFLICT;

  FinalizeRemoteSync(param.Pass(), status);
}

void DriveFileSyncService::AbortRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    SyncStatusCode status) {
  FinalizeRemoteSync(param.Pass(), status);
}

void DriveFileSyncService::FinalizeRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    SyncStatusCode status) {
  // Clear the local changes. If the operation was resolve-to-local, we should
  // not clear them here since we added the fake local change to sync with the
  // remote file.
  if (param->clear_local_changes) {
    const FileSystemURL& url = param->remote_change.url;
    param->clear_local_changes = false;
    remote_change_processor_->ClearLocalChanges(
        url, base::Bind(&DriveFileSyncService::FinalizeRemoteSync,
                        AsWeakPtr(), base::Passed(&param), status));
    return;
  }

  if (!param->temporary_file_path.empty())
    DeleteTemporaryFile(param->temporary_file_path);
  NotifyTaskDone(status, param->token.Pass());
  if (status == SYNC_STATUS_OK && param->sync_action != SYNC_ACTION_NONE) {
    NotifyObserversFileStatusChanged(param->remote_change.url,
                                     SYNC_FILE_STATUS_SYNCED,
                                     param->sync_action,
                                     SYNC_DIRECTION_REMOTE_TO_LOCAL);
  }
  param->callback.Run(status, param->remote_change.url);
}

void DriveFileSyncService::HandleConflictForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    const base::Time& remote_updated_time,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    AbortRemoteSync(param.Pass(), status);
    return;
  }
  if (!remote_updated_time.is_null())
    param->remote_change.updated_time = remote_updated_time;
  DCHECK(param);
  const FileSystemURL& url = param->remote_change.url;
  SyncFileMetadata& local_metadata = param->local_metadata;
  DriveMetadata& drive_metadata = param->drive_metadata;
  if (conflict_resolution_ == CONFLICT_RESOLUTION_MANUAL) {
    param->sync_action = SYNC_ACTION_NONE;
    MarkConflict(url, &drive_metadata,
                 base::Bind(&DriveFileSyncService::CompleteRemoteSync,
                            AsWeakPtr(), base::Passed(&param)));
    return;
  }

  DCHECK_EQ(CONFLICT_RESOLUTION_LAST_WRITE_WIN, conflict_resolution_);
  if (param->remote_change.updated_time.is_null()) {
    // Get remote file time and call this method again.
    sync_client_->GetResourceEntry(
        param->remote_change.resource_id, base::Bind(
            &DriveFileSyncService::DidGetRemoteFileMetadataForRemoteUpdatedTime,
            AsWeakPtr(),
            base::Bind(&DriveFileSyncService::HandleConflictForRemoteSync,
                       AsWeakPtr(), base::Passed(&param))));
    return;
  }
  if (local_metadata.last_modified >= param->remote_change.updated_time) {
    // Local win case.
    DVLOG(1) << "Resolving conflict for remote sync:"
             << url.DebugString() << ": LOCAL WIN";
    ResolveConflictToLocalForRemoteSync(param.Pass());
    return;
  }
  // Remote win case.
  // Make sure we reset the conflict flag and start over the remote sync
  // with empty local changes.
  DVLOG(1) << "Resolving conflict for remote sync:"
           << url.DebugString() << ": REMOTE WIN";
  drive_metadata.set_conflicted(false);
  drive_metadata.set_to_be_fetched(false);
  metadata_store_->UpdateEntry(
      url, drive_metadata,
      base::Bind(&DriveFileSyncService::StartOverRemoteSync,
                 AsWeakPtr(), base::Passed(&param)));
  return;
}

void DriveFileSyncService::ResolveConflictToLocalForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param) {
  DCHECK(param);
  const FileSystemURL& url = param->remote_change.url;
  param->sync_action = SYNC_ACTION_NONE;
  param->clear_local_changes = false;
  remote_change_processor_->RecordFakeLocalChange(
      url,
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE),
      base::Bind(&DriveFileSyncService::DidResolveConflictToLocalChange,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::StartOverRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    SyncStatusCode status) {
  DCHECK(param);
  SyncFileMetadata& local_metadata = param->local_metadata;
  DidPrepareForProcessRemoteChange(
      param.Pass(), status, local_metadata, FileChangeList());
}

bool DriveFileSyncService::AppendRemoteChange(
    const GURL& origin,
    const google_apis::ResourceEntry& entry,
    int64 changestamp,
    RemoteSyncType sync_type) {
  // TODO(tzik): Normalize the path here.
  base::FilePath path = base::FilePath::FromUTF8Unsafe(entry.title());
  if (entry.is_folder())
    return false;
  return AppendRemoteChangeInternal(
      origin, path, entry.deleted(),
      entry.resource_id(), changestamp,
      entry.deleted() ? std::string() : entry.file_md5(),
      entry.updated_time(), sync_type);
}

bool DriveFileSyncService::AppendFetchChange(
    const GURL& origin,
    const base::FilePath& path,
    const std::string& resource_id) {
  return AppendRemoteChangeInternal(
      origin, path,
      false,  // is_deleted
      resource_id,
      0,  // changestamp
      std::string(),  // remote_file_md5
      base::Time(),  // updated_time
      REMOTE_SYNC_TYPE_FETCH);
}

bool DriveFileSyncService::AppendRemoteChangeInternal(
    const GURL& origin,
    const base::FilePath& path,
    bool is_deleted,
    const std::string& remote_resource_id,
    int64 changestamp,
    const std::string& remote_file_md5,
    const base::Time& updated_time,
    RemoteSyncType sync_type) {
  fileapi::FileSystemURL url(
      CreateSyncableFileSystemURL(origin, kServiceName, path));
  DCHECK(url.is_valid());

  std::string local_resource_id;
  std::string local_file_md5;

  DriveMetadata metadata;
  bool has_db_entry =
      (metadata_store_->ReadEntry(url, &metadata) == SYNC_STATUS_OK);
  if (has_db_entry) {
    local_resource_id = metadata.resource_id();
    if (!metadata.to_be_fetched())
      local_file_md5 = metadata.md5_checksum();
  }

  PathToChangeMap* path_to_change = &origin_to_changes_map_[origin];
  PathToChangeMap::iterator found = path_to_change->find(path);
  PendingChangeQueue::iterator overridden_queue_item = pending_changes_.end();
  if (found != path_to_change->end()) {
    if (found->second.changestamp >= changestamp)
      return false;
    const RemoteChange& pending_change = found->second;
    overridden_queue_item = pending_change.position_in_queue;

    if (pending_change.change.IsDelete()) {
      local_resource_id.clear();
      local_file_md5.clear();
    } else {
      local_resource_id = pending_change.resource_id;
      local_file_md5 = pending_change.md5_checksum;
    }
  }

  // Drop the change if remote update change does not change file content.
  if (!remote_file_md5.empty() &&
      !local_file_md5.empty() &&
      remote_file_md5 == local_file_md5)
    return false;

  // Drop any change if the change has unknown resource id.
  if (!remote_resource_id.empty() &&
      !local_resource_id.empty() &&
      remote_resource_id != local_resource_id)
    return false;

  FileChange file_change(CreateFileChange(is_deleted));

  if (is_deleted && has_db_entry) {
    metadata.set_resource_id(std::string());
    metadata_store_->UpdateEntry(url, metadata,
                                 base::Bind(&EmptyStatusCallback));
  }

  // Do not return in this block. These changes should be done together.
  {
    if (overridden_queue_item != pending_changes_.end())
      pending_changes_.erase(overridden_queue_item);

    std::pair<PendingChangeQueue::iterator, bool> inserted_to_queue =
        pending_changes_.insert(ChangeQueueItem(changestamp, sync_type, url));
    DCHECK(inserted_to_queue.second);

    (*path_to_change)[path] = RemoteChange(
        changestamp, remote_resource_id, remote_file_md5,
        updated_time, sync_type, url, file_change,
        inserted_to_queue.first);
  }

  DVLOG(3) << "Append remote change: " << path.value()
           << "@" << changestamp << " "
           << file_change.DebugString();

  return true;
}

void DriveFileSyncService::RemoveRemoteChange(
    const FileSystemURL& url) {
  OriginToChangesMap::iterator found_origin =
      origin_to_changes_map_.find(url.origin());
  if (found_origin == origin_to_changes_map_.end())
    return;

  PathToChangeMap* path_to_change = &found_origin->second;
  PathToChangeMap::iterator found_change = path_to_change->find(url.path());
  if (found_change == path_to_change->end())
    return;

  pending_changes_.erase(found_change->second.position_in_queue);
  path_to_change->erase(found_change);
  if (path_to_change->empty())
    origin_to_changes_map_.erase(found_origin);

  if (metadata_store_->IsBatchSyncOrigin(url.origin()) &&
      !ContainsKey(origin_to_changes_map_, url.origin())) {
    metadata_store_->MoveBatchSyncOriginToIncremental(url.origin());
  }
}

bool DriveFileSyncService::GetPendingChangeForFileSystemURL(
    const FileSystemURL& url,
    RemoteChange* change) const {
  DCHECK(change);
  OriginToChangesMap::const_iterator found_url =
      origin_to_changes_map_.find(url.origin());
  if (found_url == origin_to_changes_map_.end())
    return false;
  const PathToChangeMap& path_to_change = found_url->second;
  PathToChangeMap::const_iterator found_path = path_to_change.find(url.path());
  if (found_path == path_to_change.end())
    return false;
  *change = found_path->second;
  return true;
}

void DriveFileSyncService::MarkConflict(
    const fileapi::FileSystemURL& url,
    DriveMetadata* drive_metadata,
    const SyncStatusCallback& callback) {
  DCHECK(drive_metadata);
  if (drive_metadata->resource_id().empty()) {
    // If the file does not have valid drive_metadata in the metadata store
    // we must have a pending remote change entry.
    RemoteChange remote_change;
    const bool has_remote_change =
        GetPendingChangeForFileSystemURL(url, &remote_change);
    DCHECK(has_remote_change);
    drive_metadata->set_resource_id(remote_change.resource_id);
    drive_metadata->set_md5_checksum(std::string());
  }
  drive_metadata->set_conflicted(true);
  drive_metadata->set_to_be_fetched(false);
  metadata_store_->UpdateEntry(url, *drive_metadata, callback);
  NotifyObserversFileStatusChanged(url,
                                   SYNC_FILE_STATUS_CONFLICTING,
                                   SYNC_ACTION_NONE,
                                   SYNC_DIRECTION_NONE);
}

void DriveFileSyncService::DidGetRemoteFileMetadataForRemoteUpdatedTime(
    const UpdatedTimeCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status == SYNC_FILE_ERROR_NOT_FOUND) {
    // Returns with very old (time==0.0) last modified date
    // so that last-write-win policy will just use the other (local) version.
    callback.Run(base::Time::FromDoubleT(0.0), SYNC_STATUS_OK);
    return;
  }
  callback.Run(entry->updated_time(), status);
}

SyncStatusCode DriveFileSyncService::GDataErrorCodeToSyncStatusCodeWrapper(
    google_apis::GDataErrorCode error) const {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK && !sync_client_->IsAuthenticated())
    return SYNC_STATUS_AUTHENTICATION_FAILED;
  return status;
}

void DriveFileSyncService::MaybeStartFetchChanges() {
  if (!token_ || GetCurrentState() == REMOTE_SERVICE_DISABLED) {
    // If another task is already running or the service is disabled
    // just return here.
    // Note that token_ should be already non-null if this is called
    // from NotifyTaskDone().
    return;
  }

  // If we have pending_batch_sync_origins, try starting the batch sync.
  if (!pending_batch_sync_origins_.empty()) {
    if (GetCurrentState() == REMOTE_SERVICE_OK ||
        may_have_unfetched_changes_) {
      GURL origin = *pending_batch_sync_origins_.begin();
      pending_batch_sync_origins_.erase(pending_batch_sync_origins_.begin());
      std::string resource_id = metadata_store_->GetResourceIdForOrigin(origin);
      DCHECK(!resource_id.empty());
      StartBatchSyncForOrigin(origin, resource_id);
    }
    return;
  }

  if (may_have_unfetched_changes_ &&
      !metadata_store_->incremental_sync_origins().empty()) {
    FetchChangesForIncrementalSync();
  }
}

void DriveFileSyncService::FetchChangesForIncrementalSync() {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE, TASK_TYPE_DRIVE,
                                       "Fetching remote change list"));
  DCHECK(token);
  DCHECK(may_have_unfetched_changes_);
  DCHECK(pending_batch_sync_origins_.empty());
  DCHECK(!metadata_store_->incremental_sync_origins().empty());

  DVLOG(1) << "FetchChangesForIncrementalSync (start_changestamp:"
           << (largest_fetched_changestamp_ + 1) << ")";

  sync_client_->ListChanges(
      largest_fetched_changestamp_ + 1,
      base::Bind(&DriveFileSyncService::DidFetchChangesForIncrementalSync,
                 AsWeakPtr(), base::Passed(&token), false));

  may_have_unfetched_changes_ = false;
}

void DriveFileSyncService::DidFetchChangesForIncrementalSync(
    scoped_ptr<TaskToken> token,
    bool has_new_changes,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> changes) {
  if (error != google_apis::HTTP_SUCCESS) {
    NotifyTaskDone(GDataErrorCodeToSyncStatusCodeWrapper(error), token.Pass());
    return;
  }

  bool reset_sync_root = false;
  std::set<GURL> reset_origins;

  typedef ScopedVector<google_apis::ResourceEntry>::const_iterator iterator;
  for (iterator itr = changes->entries().begin();
       itr != changes->entries().end(); ++itr) {
    const google_apis::ResourceEntry& entry = **itr;

    if (entry.deleted()) {
      // Check if the sync root or origin root folder is deleted.
      // (We reset resource_id after the for loop so that we can handle
      // recursive delete for the origin (at least in this feed)
      // while GetOriginForEntry for the origin still works.)
      if (entry.resource_id() == sync_root_resource_id()) {
        reset_sync_root = true;
        continue;
      }
      GURL origin;
      if (metadata_store_->GetOriginByOriginRootDirectoryId(
              entry.resource_id(), &origin)) {
        reset_origins.insert(origin);
        continue;
      }
    }

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

  if (reset_sync_root) {
    LOG(WARNING) << "Detected unexpected SyncRoot deletion.";
    metadata_store_->SetSyncRootDirectory(std::string());
  }
  for (std::set<GURL>::iterator itr = reset_origins.begin();
       itr != reset_origins.end(); ++itr) {
    LOG(WARNING) << "Detected unexpected OriginRoot deletion:" << itr->spec();
    pending_batch_sync_origins_.erase(*itr);
    metadata_store_->SetOriginRootDirectory(*itr, std::string());
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
    UpdatePollingDelay(kMinimumPollingDelaySeconds);
  } else {
    // If the change_queue_ was not updated, update the polling delay to wait
    // longer.
    UpdatePollingDelay(static_cast<int64>(
        kDelayMultiplier * polling_delay_seconds_));
  }

  NotifyTaskDone(SYNC_STATUS_OK, token.Pass());
}

bool DriveFileSyncService::GetOriginForEntry(
    const google_apis::ResourceEntry& entry,
    GURL* origin_out) {
  typedef ScopedVector<google_apis::Link>::const_iterator iterator;
  for (iterator itr = entry.links().begin();
       itr != entry.links().end(); ++itr) {
    if ((*itr)->type() != google_apis::Link::LINK_PARENT)
      continue;
    GURL origin(DriveFileSyncClient::DirectoryTitleToOrigin((*itr)->title()));
    DCHECK(origin.is_valid());

    if (!metadata_store_->IsBatchSyncOrigin(origin) &&
        !metadata_store_->IsIncrementalSyncOrigin(origin))
      continue;
    std::string resource_id(metadata_store_->GetResourceIdForOrigin(origin));
    if (resource_id.empty())
      continue;
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
  if (polling_timer_.IsRunning() ||
      polling_delay_seconds_ < 0 ||
      GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  DVLOG(1) << "Polling scheduled"
           << " (delay:" << polling_delay_seconds_ << "s)";

  polling_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(polling_delay_seconds_),
      base::Bind(&DriveFileSyncService::OnPollingTimerFired, AsWeakPtr()));
}

void DriveFileSyncService::OnPollingTimerFired() {
  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

void DriveFileSyncService::UpdatePollingDelay(int64 new_delay_sec) {
  // polling_delay_seconds_ made negative to disable polling for testing.
  if (polling_delay_seconds_ < 0)
    return;

  if (state_ == REMOTE_SERVICE_TEMPORARY_UNAVAILABLE) {
    // If the service state is TEMPORARY_UNAVAILABLE, poll the service
    // with a modest duration (but more frequently than
    // kPollingDelaySecondsWithNotification) so that we have a mild chance
    // to recover the state.
    polling_delay_seconds_ = kMaximumPollingDelaySeconds;
    polling_timer_.Stop();
    return;
  }

  if (push_notification_enabled_) {
    polling_delay_seconds_ = kPollingDelaySecondsWithNotification;
    return;
  }

  int64 old_delay = polling_delay_seconds_;

  // Push notifications off.
  polling_delay_seconds_ = std::min(new_delay_sec, kMaximumPollingDelaySeconds);

  if (polling_delay_seconds_ < old_delay)
    polling_timer_.Stop();
}

bool DriveFileSyncService::IsDriveNotificationSupported() {
  // TODO(calvinlo): A invalidation ID can only be registered to one handler.
  // Therefore ChromeOS and SyncFS cannot both use XMPP notifications until
  // (http://crbug.com/173339) is completed.
  // For now, disable XMPP notifications for SyncFC on ChromeOS to guarantee
  // that ChromeOS's file manager can register itself to the invalidationID.

#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

// Register for Google Drive invalidation notifications through XMPP.
void DriveFileSyncService::RegisterDriveNotifications() {
  // Push notification registration might have already occurred if called from
  // a different extension.
  if (!IsDriveNotificationSupported() || push_notification_registered_)
    return;

  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (!profile_sync_service)
    return;

  profile_sync_service->RegisterInvalidationHandler(this);
  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::COSMO_CHANGELOG,
      kDriveInvalidationObjectId));
  profile_sync_service->UpdateRegisteredInvalidationIds(this, ids);
  push_notification_registered_ = true;
  SetPushNotificationEnabled(profile_sync_service->GetInvalidatorState());
}

void DriveFileSyncService::SetPushNotificationEnabled(
    syncer::InvalidatorState state) {
  push_notification_enabled_ = (state == syncer::INVALIDATIONS_ENABLED);
  if (!push_notification_enabled_)
    return;

  // Push notifications are enabled so reset polling timer.
  UpdatePollingDelay(kPollingDelaySecondsWithNotification);
}

void DriveFileSyncService::NotifyObserversFileStatusChanged(
    const FileSystemURL& url,
    SyncFileStatus sync_status,
    SyncAction action_taken,
    SyncDirection direction) {
  if (sync_status != SYNC_FILE_STATUS_SYNCED) {
    DCHECK_EQ(SYNC_ACTION_NONE, action_taken);
    DCHECK_EQ(SYNC_DIRECTION_NONE, direction);
  }

  FOR_EACH_OBSERVER(
      FileStatusObserver, file_status_observers_,
      OnFileStatusChanged(url, sync_status, action_taken, direction));
}

void DriveFileSyncService::EnsureSyncRootDirectory(
    const ResourceIdCallback& callback) {
  if (!sync_root_resource_id().empty()) {
    callback.Run(SYNC_STATUS_OK, sync_root_resource_id());
    return;
  }

  sync_client_->GetDriveDirectoryForSyncRoot(base::Bind(
      &DriveFileSyncService::DidEnsureSyncRoot, AsWeakPtr(), callback));
}

void DriveFileSyncService::DidEnsureSyncRoot(
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& sync_root_resource_id) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status == SYNC_STATUS_OK)
    metadata_store_->SetSyncRootDirectory(sync_root_resource_id);
  callback.Run(status, sync_root_resource_id);
}

void DriveFileSyncService::EnsureOriginRootDirectory(
    const GURL& origin,
    const ResourceIdCallback& callback) {
  std::string resource_id = metadata_store_->GetResourceIdForOrigin(origin);
  if (!resource_id.empty()) {
    callback.Run(SYNC_STATUS_OK, resource_id);
    return;
  }

  EnsureSyncRootDirectory(base::Bind(
      &DriveFileSyncService::DidEnsureSyncRootForOriginRoot,
      AsWeakPtr(), origin, callback));
}

void DriveFileSyncService::DidEnsureSyncRootForOriginRoot(
    const GURL& origin,
    const ResourceIdCallback& callback,
    SyncStatusCode status,
    const std::string& sync_root_resource_id) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status, std::string());
    return;
  }

  sync_client_->GetDriveDirectoryForOrigin(
      sync_root_resource_id, origin,
      base::Bind(&DriveFileSyncService::DidEnsureOriginRoot,
                 AsWeakPtr(), origin, callback));
}

void DriveFileSyncService::DidEnsureOriginRoot(
    const GURL& origin,
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status == SYNC_STATUS_OK &&
      metadata_store_->IsKnownOrigin(origin)) {
    metadata_store_->SetOriginRootDirectory(origin, resource_id);
    if (metadata_store_->IsBatchSyncOrigin(origin))
      pending_batch_sync_origins_.insert(origin);
  }
  callback.Run(status, resource_id);
}

std::string DriveFileSyncService::sync_root_resource_id() {
  return metadata_store_->sync_root_directory();
}

}  // namespace sync_file_system

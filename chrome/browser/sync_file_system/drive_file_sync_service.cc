// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_notification_manager.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/drive/api_util.h"
#include "chrome/browser/sync_file_system/drive/local_change_processor_delegate.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/remote_change_handler.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/remote_sync_operation_resolver.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/syncable/sync_file_metadata.h"
#include "webkit/browser/fileapi/syncable/sync_file_type.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/common/blob/scoped_file.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

typedef DriveFileSyncService::ConflictResolutionResult ConflictResolutionResult;
typedef RemoteFileSyncService::OriginStatusMap OriginStatusMap;

namespace {

const base::FilePath::CharType kTempDirName[] = FILE_PATH_LITERAL("tmp");
const base::FilePath::CharType kSyncFileSystemDir[] =
    FILE_PATH_LITERAL("Sync FileSystem");
const base::FilePath::CharType kSyncFileSystemDirDev[] =
    FILE_PATH_LITERAL("Sync FileSystem Dev");

const base::FilePath::CharType* GetSyncFileSystemDir() {
  return IsSyncFSDirectoryOperationEnabled()
      ? kSyncFileSystemDirDev : kSyncFileSystemDir;
}

bool CreateTemporaryFile(const base::FilePath& dir_path,
                         webkit_blob::ScopedFile* temp_file) {
  base::FilePath temp_file_path;
  const bool success = file_util::CreateDirectory(dir_path) &&
      file_util::CreateTemporaryFileInDir(dir_path, &temp_file_path);
  if (!success)
    return success;
  *temp_file =
      webkit_blob::ScopedFile(temp_file_path,
                              webkit_blob::ScopedFile::DELETE_ON_SCOPE_OUT,
                              base::MessageLoopProxy::current().get());
  return success;
}

void EmptyStatusCallback(SyncStatusCode status) {}

void SyncFileCallbackAdapter(
    const SyncStatusCallback& status_callback,
    const SyncFileCallback& callback,
    SyncStatusCode status,
    const FileSystemURL& url) {
  status_callback.Run(status);
  callback.Run(status, url);
}

}  // namespace

ConflictResolutionPolicy DriveFileSyncService::kDefaultPolicy =
    CONFLICT_RESOLUTION_LAST_WRITE_WIN;

struct DriveFileSyncService::ProcessRemoteChangeParam {
  RemoteChangeHandler::RemoteChange remote_change;
  SyncFileCallback callback;

  DriveMetadata drive_metadata;
  SyncFileMetadata local_metadata;
  bool metadata_updated;
  webkit_blob::ScopedFile temporary_file;
  std::string md5_checksum;
  SyncAction sync_action;
  bool clear_local_changes;

  ProcessRemoteChangeParam(
      const RemoteChangeHandler::RemoteChange& remote_change,
      const SyncFileCallback& callback)
      : remote_change(remote_change),
        callback(callback),
        metadata_updated(false),
        sync_action(SYNC_ACTION_NONE),
        clear_local_changes(true) {
  }
};

// DriveFileSyncService ------------------------------------------------------

DriveFileSyncService::~DriveFileSyncService() {
  if (api_util_)
    api_util_->RemoveObserver(this);

  ::drive::DriveNotificationManager* drive_notification_manager =
      ::drive::DriveNotificationManagerFactory::GetForProfile(profile_);
  if (drive_notification_manager)
    drive_notification_manager->RemoveObserver(this);
}

scoped_ptr<DriveFileSyncService> DriveFileSyncService::Create(
    Profile* profile) {
  scoped_ptr<DriveFileSyncService> service(new DriveFileSyncService(profile));
  scoped_ptr<SyncTaskManager> task_manager(
      new SyncTaskManager(service->AsWeakPtr()));
  SyncStatusCallback callback = base::Bind(
      &SyncTaskManager::Initialize, task_manager->AsWeakPtr());
  service->Initialize(task_manager.Pass(), callback);
  return service.Pass();
}

scoped_ptr<DriveFileSyncService> DriveFileSyncService::CreateForTesting(
    Profile* profile,
    const base::FilePath& base_dir,
    scoped_ptr<drive::APIUtilInterface> api_util,
    scoped_ptr<DriveMetadataStore> metadata_store) {
  scoped_ptr<DriveFileSyncService> service(new DriveFileSyncService(profile));
  scoped_ptr<SyncTaskManager> task_manager(
      new SyncTaskManager(service->AsWeakPtr()));
  SyncStatusCallback callback = base::Bind(
      &SyncTaskManager::Initialize, task_manager->AsWeakPtr());
  service->InitializeForTesting(task_manager.Pass(),
                                base_dir,
                                api_util.Pass(),
                                metadata_store.Pass(),
                                callback);
  return service.Pass();
}

scoped_ptr<drive::APIUtilInterface>
DriveFileSyncService::DestroyAndPassAPIUtilForTesting(
    scoped_ptr<DriveFileSyncService> sync_service) {
  return sync_service->api_util_.Pass();
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
  task_manager_->ScheduleTask(
      base::Bind(&DriveFileSyncService::DoRegisterOriginForTrackingChanges,
                 AsWeakPtr(), origin),
      callback);
}

void DriveFileSyncService::UnregisterOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTask(
      base::Bind(&DriveFileSyncService::DoUnregisterOriginForTrackingChanges,
                 AsWeakPtr(), origin),
      callback);
}

void DriveFileSyncService::EnableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTask(
      base::Bind(&DriveFileSyncService::DoEnableOriginForTrackingChanges,
                 AsWeakPtr(), origin),
      callback);
}

void DriveFileSyncService::DisableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTask(
      base::Bind(&DriveFileSyncService::DoDisableOriginForTrackingChanges,
                 AsWeakPtr(), origin),
      callback);
}

void DriveFileSyncService::UninstallOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTask(
      base::Bind(&DriveFileSyncService::DoUninstallOrigin, AsWeakPtr(), origin),
      callback);
}

void DriveFileSyncService::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  task_manager_->ScheduleTask(
      base::Bind(&DriveFileSyncService::DoProcessRemoteChange, AsWeakPtr(),
                 callback),
      base::Bind(&EmptyStatusCallback));
}

void DriveFileSyncService::SetRemoteChangeProcessor(
    RemoteChangeProcessor* processor) {
  remote_change_processor_ = processor;
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

RemoteServiceState DriveFileSyncService::GetCurrentState() const {
  if (!sync_enabled_)
    return REMOTE_SERVICE_DISABLED;
  return state_;
}

void DriveFileSyncService::GetOriginStatusMap(OriginStatusMap* status_map) {
  DCHECK(status_map);

  // Add batch sync origins held by DriveFileSyncService.
  typedef std::map<GURL, std::string>::const_iterator iterator;
  for (iterator itr = pending_batch_sync_origins_.begin();
       itr != pending_batch_sync_origins_.end();
       ++itr)
    (*status_map)[itr->first] = "Pending";

  // Add incremental and disabled origins held by DriveMetadataStore.
  for (iterator itr = metadata_store_->incremental_sync_origins().begin();
       itr != metadata_store_->incremental_sync_origins().end();
       ++itr)
    (*status_map)[itr->first] = "Enabled";

  for (iterator itr = metadata_store_->disabled_origins().begin();
       itr != metadata_store_->disabled_origins().end();
       ++itr)
    (*status_map)[itr->first] = "Disabled";
}

void DriveFileSyncService::GetFileMetadataMap(
    OriginFileMetadataMap* metadata_map) {
  DCHECK(metadata_map);
  metadata_store_->GetFileMetadataMap(metadata_map);
}

void DriveFileSyncService::SetSyncEnabled(bool enabled) {
  if (sync_enabled_ == enabled)
    return;

  RemoteServiceState old_state = GetCurrentState();
  sync_enabled_ = enabled;
  if (old_state == GetCurrentState())
    return;

  const char* status_message = enabled ? "Sync is enabled" : "Sync is disabled";
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), status_message));
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
  task_manager_->ScheduleTask(
      base::Bind(&DriveFileSyncService::DoApplyLocalChange, AsWeakPtr(),
                 local_file_change,
                 local_file_path,
                 local_file_metadata,
                 url),
      callback);
}

void DriveFileSyncService::OnAuthenticated() {
  if (state_ == REMOTE_SERVICE_OK)
    return;
  util::Log(logging::LOG_INFO, FROM_HERE, "OnAuthenticated");

  UpdateServiceState(REMOTE_SERVICE_OK, "Authenticated");

  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

void DriveFileSyncService::OnNetworkConnected() {
  if (state_ == REMOTE_SERVICE_OK)
    return;
  util::Log(logging::LOG_INFO, FROM_HERE, "OnNetworkConnected");

  UpdateServiceState(REMOTE_SERVICE_OK, "Network connected");

  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

DriveFileSyncService::DriveFileSyncService(Profile* profile)
    : profile_(profile),
      state_(REMOTE_SERVICE_OK),
      sync_enabled_(true),
      largest_fetched_changestamp_(0),
      may_have_unfetched_changes_(false),
      remote_change_processor_(NULL),
      last_gdata_error_(google_apis::HTTP_SUCCESS),
      conflict_resolution_(kDefaultPolicy) {
}

void DriveFileSyncService::Initialize(
    scoped_ptr<SyncTaskManager> task_manager,
    const SyncStatusCallback& callback) {
  DCHECK(profile_);
  DCHECK(!metadata_store_);
  DCHECK(!task_manager_);

  task_manager_ = task_manager.Pass();

  temporary_file_dir_ =
      profile_->GetPath().Append(GetSyncFileSystemDir()).Append(kTempDirName);

  api_util_.reset(new drive::APIUtil(profile_));
  api_util_->AddObserver(this);

  metadata_store_.reset(new DriveMetadataStore(
      profile_->GetPath().Append(GetSyncFileSystemDir()),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE).get()));

  metadata_store_->Initialize(
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore,
                 AsWeakPtr(), callback));
}

void DriveFileSyncService::InitializeForTesting(
    scoped_ptr<SyncTaskManager> task_manager,
    const base::FilePath& base_dir,
    scoped_ptr<drive::APIUtilInterface> api_util,
    scoped_ptr<DriveMetadataStore> metadata_store,
    const SyncStatusCallback& callback) {
  DCHECK(!metadata_store_);
  DCHECK(!task_manager_);

  task_manager_ = task_manager.Pass();
  temporary_file_dir_ = base_dir.Append(kTempDirName);

  api_util_ = api_util.Pass();
  metadata_store_ = metadata_store.Pass();

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore,
                 AsWeakPtr(), callback, SYNC_STATUS_OK, false));
}

void DriveFileSyncService::DidInitializeMetadataStore(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    bool created) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  DCHECK(pending_batch_sync_origins_.empty());

  UpdateRegisteredOrigins();

  largest_fetched_changestamp_ = metadata_store_->GetLargestChangeStamp();

  DriveMetadataStore::URLAndDriveMetadataList to_be_fetched_files;
  status = metadata_store_->GetToBeFetchedFiles(&to_be_fetched_files);
  DCHECK_EQ(SYNC_STATUS_OK, status);
  typedef DriveMetadataStore::URLAndDriveMetadataList::const_iterator iterator;
  for (iterator itr = to_be_fetched_files.begin();
       itr != to_be_fetched_files.end(); ++itr) {
    const FileSystemURL& url = itr->first;
    const DriveMetadata& metadata = itr->second;
    const std::string& resource_id = metadata.resource_id();

    SyncFileType file_type = SYNC_FILE_TYPE_FILE;
    if (metadata.type() == DriveMetadata::RESOURCE_TYPE_FOLDER)
      file_type = SYNC_FILE_TYPE_DIRECTORY;
    if (!metadata_store_->IsIncrementalSyncOrigin(url.origin())) {
      metadata_store_->DeleteEntry(url, base::Bind(&EmptyStatusCallback));
      continue;
    }
    AppendFetchChange(url.origin(), url.path(), resource_id, file_type);
  }

  if (!sync_root_resource_id().empty())
    api_util_->EnsureSyncRootIsNotInMyDrive(sync_root_resource_id());

  callback.Run(status);
  may_have_unfetched_changes_ = true;

  ::drive::DriveNotificationManager* drive_notification_manager =
      ::drive::DriveNotificationManagerFactory::GetForProfile(profile_);
  if (drive_notification_manager)
    drive_notification_manager->AddObserver(this);
}

void DriveFileSyncService::UpdateServiceStateFromLastOperationStatus(
    SyncStatusCode sync_status,
    google_apis::GDataErrorCode gdata_error) {
  switch (sync_status) {
    case SYNC_STATUS_OK:
      // If the last Drive-related operation was successful we can
      // change the service state to OK.
      if (GDataErrorCodeToSyncStatusCode(gdata_error) == SYNC_STATUS_OK)
        UpdateServiceState(REMOTE_SERVICE_OK, std::string());
      break;

    // Authentication error.
    case SYNC_STATUS_AUTHENTICATION_FAILED:
      UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                         "Authentication required");
      break;

    // OAuth token error.
    case SYNC_STATUS_ACCESS_FORBIDDEN:
      UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                         "Access forbidden");
      break;

    // Errors which could make the service temporarily unavailable.
    case SYNC_STATUS_RETRY:
    case SYNC_STATUS_NETWORK_ERROR:
    case SYNC_STATUS_ABORT:
    case SYNC_STATUS_FAILED:
      UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
                         "Network or temporary service error.");
      break;

    // Errors which would require manual user intervention to resolve.
    case SYNC_DATABASE_ERROR_CORRUPTION:
    case SYNC_DATABASE_ERROR_IO_ERROR:
    case SYNC_DATABASE_ERROR_FAILED:
      UpdateServiceState(REMOTE_SERVICE_DISABLED,
                         "Unrecoverable database error");
      break;

    default:
      // Other errors don't affect service state
      break;
  }
}

void DriveFileSyncService::UpdateServiceState(RemoteServiceState state,
                                              const std::string& description) {
  RemoteServiceState old_state = GetCurrentState();
  state_ = state;

  // Notify remote sync service state if the state has been changed.
  if (old_state != GetCurrentState()) {
    util::Log(logging::LOG_INFO, FROM_HERE,
              "Service state changed: %d->%d: %s",
              old_state, GetCurrentState(), description.c_str());
    FOR_EACH_OBSERVER(
        Observer, service_observers_,
        OnRemoteServiceStateUpdated(GetCurrentState(), description));
  }
}

void DriveFileSyncService::DoRegisterOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));

  DCHECK(!metadata_store_->IsOriginDisabled(origin));
  if (!metadata_store_->GetResourceIdForOrigin(origin).empty()) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  EnsureOriginRootDirectory(
      origin, base::Bind(&DriveFileSyncService::DidGetDriveDirectoryForOrigin,
                         AsWeakPtr(), origin, callback));
}

void DriveFileSyncService::DoUnregisterOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  remote_change_handler_.RemoveChangesForOrigin(origin);
  pending_batch_sync_origins_.erase(origin);
  metadata_store_->RemoveOrigin(origin, callback);
}

void DriveFileSyncService::DoEnableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  // If origin cannot be found in disabled list, then it's not a SyncFS app
  // and should be ignored.
  if (!metadata_store_->IsOriginDisabled(origin)) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  pending_batch_sync_origins_.insert(
      *metadata_store_->disabled_origins().find(origin));
  metadata_store_->EnableOrigin(origin, callback);
}

void DriveFileSyncService::DoDisableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  pending_batch_sync_origins_.erase(origin);
  if (!metadata_store_->IsIncrementalSyncOrigin(origin)) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  remote_change_handler_.RemoveChangesForOrigin(origin);
  metadata_store_->DisableOrigin(origin, callback);
}

void DriveFileSyncService::DoUninstallOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  // Because origin management is now split between DriveFileSyncService and
  // DriveMetadataStore, resource_id must be checked for in two places.
  std::string resource_id = metadata_store_->GetResourceIdForOrigin(origin);
  if (resource_id.empty()) {
    std::map<GURL, std::string>::const_iterator iterator =
        pending_batch_sync_origins_.find(origin);
    if (iterator != pending_batch_sync_origins_.end())
      resource_id = iterator->second;
  }

  // An empty resource_id indicates either one of following two cases:
  // 1) origin is not in metadata_store_ because the extension was never
  //    run or it's not managed by this service, and thus no
  //    origin directory on the remote drive was created.
  // 2) origin or sync root folder is deleted on Drive.
  if (resource_id.empty()) {
    callback.Run(SYNC_STATUS_UNKNOWN_ORIGIN);
    return;
  }

  // Convert origin's directory GURL to ResourceID and delete it. Expected MD5
  // is empty to force delete (i.e. skip conflict resolution).
  api_util_->DeleteFile(resource_id,
                        std::string(),
                        base::Bind(&DriveFileSyncService::DidUninstallOrigin,
                                   AsWeakPtr(),
                                   origin,
                                   callback));
}

void DriveFileSyncService::DoProcessRemoteChange(
    const SyncFileCallback& sync_callback,
    const SyncStatusCallback& completion_callback) {
  DCHECK(remote_change_processor_);

  SyncFileCallback callback =
      base::Bind(&SyncFileCallbackAdapter, completion_callback, sync_callback);

  if (!remote_change_handler_.HasChanges()) {
    callback.Run(SYNC_STATUS_NO_CHANGE_TO_SYNC, FileSystemURL());
    return;
  }

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED) {
    callback.Run(SYNC_STATUS_SYNC_DISABLED, FileSystemURL());
    return;
  }

  RemoteChangeHandler::RemoteChange remote_change;
  bool has_remote_change =
      remote_change_handler_.GetChange(&remote_change);
  DCHECK(has_remote_change);

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "ProcessRemoteChange for %s change:%s",
            remote_change.url.DebugString().c_str(),
            remote_change.change.DebugString().c_str());

  scoped_ptr<ProcessRemoteChangeParam> param(new ProcessRemoteChangeParam(
      remote_change, callback));
  remote_change_processor_->PrepareForProcessRemoteChange(
      remote_change.url,
      base::Bind(&DriveFileSyncService::DidPrepareForProcessRemoteChange,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DoApplyLocalChange(
    const FileChange& local_file_change,
    const base::FilePath& local_file_path,
    const SyncFileMetadata& local_file_metadata,
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  if (GetCurrentState() == REMOTE_SERVICE_DISABLED) {
    callback.Run(SYNC_STATUS_SYNC_DISABLED);
    return;
  }

  if (!metadata_store_->IsIncrementalSyncOrigin(url.origin())) {
    // We may get called by LocalFileSyncService to sync local changes
    // for the origins that are disabled.
    DVLOG(1) << "Got request for stray origin: " << url.origin().spec();
    callback.Run(SYNC_STATUS_UNKNOWN_ORIGIN);
    return;
  }

  DCHECK(!running_local_sync_task_);
  running_local_sync_task_.reset(new drive::LocalChangeProcessorDelegate(
      this, local_file_change, local_file_path, local_file_metadata, url));
  running_local_sync_task_->Run(base::Bind(
      &DriveFileSyncService::DidApplyLocalChange, AsWeakPtr(), callback));
}

void DriveFileSyncService::UpdateRegisteredOrigins() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(pending_batch_sync_origins_.empty());
  if (!extension_service)
    return;

  std::vector<GURL> origins;
  metadata_store_->GetAllOrigins(&origins);

  // Update the status of every origin using status from ExtensionService.
  for (std::vector<GURL>::const_iterator itr = origins.begin();
       itr != origins.end(); ++itr) {
    std::string extension_id = itr->host();
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(extension_id);

    if (!extension_service->GetInstalledExtension(extension_id)) {
      // Extension has been uninstalled.
      UninstallOrigin(origin, base::Bind(&EmptyStatusCallback));
    } else if (metadata_store_->IsIncrementalSyncOrigin(origin) &&
               !extension_service->IsExtensionEnabled(extension_id)) {
      // Incremental Extension has been disabled.
      metadata_store_->DisableOrigin(origin, base::Bind(&EmptyStatusCallback));
    } else if (metadata_store_->IsOriginDisabled(origin) &&
               extension_service->IsExtensionEnabled(extension_id)) {
      // Extension has been re-enabled.
      pending_batch_sync_origins_.insert(
          *metadata_store_->disabled_origins().find(origin));
      metadata_store_->EnableOrigin(origin, base::Bind(&EmptyStatusCallback));
    }
  }
}

void DriveFileSyncService::StartBatchSync(
    const SyncStatusCallback& callback) {
  DCHECK(GetCurrentState() == REMOTE_SERVICE_OK || may_have_unfetched_changes_);
  DCHECK(!pending_batch_sync_origins_.empty());

  GURL origin = pending_batch_sync_origins_.begin()->first;
  std::string resource_id = pending_batch_sync_origins_.begin()->second;
  DCHECK(!resource_id.empty());
  pending_batch_sync_origins_.erase(pending_batch_sync_origins_.begin());

  DCHECK(!metadata_store_->IsOriginDisabled(origin));

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Start batch sync for: %s", origin.spec().c_str());

  api_util_->GetLargestChangeStamp(
      base::Bind(&DriveFileSyncService::DidGetLargestChangeStampForBatchSync,
                 AsWeakPtr(),
                 callback,
                 origin,
                 resource_id));

  may_have_unfetched_changes_ = false;
}

void DriveFileSyncService::DidGetDriveDirectoryForOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    const std::string& resource_id) {
  if (status == SYNC_FILE_ERROR_NOT_FOUND &&
      !sync_root_resource_id().empty()) {
    // Retry after (re-)creating the sync root directory.
    metadata_store_->SetSyncRootDirectory(std::string());
    EnsureOriginRootDirectory(
        origin, base::Bind(
            &DriveFileSyncService::DidGetDriveDirectoryForOrigin,
            AsWeakPtr(), origin, callback));
    return;
  }

  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  if (!metadata_store_->IsKnownOrigin(origin))
    pending_batch_sync_origins_.insert(std::make_pair(origin, resource_id));

  callback.Run(SYNC_STATUS_OK);
}

void DriveFileSyncService::DidUninstallOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status != SYNC_STATUS_OK && status != SYNC_FILE_ERROR_NOT_FOUND) {
    callback.Run(status);
    return;
  }

  // Origin directory has been removed so it's now safe to remove the origin
  // from the metadata store.
  DoUnregisterOriginForTrackingChanges(origin, callback);
}

void DriveFileSyncService::DidGetLargestChangeStampForBatchSync(
    const SyncStatusCallback& callback,
    const GURL& origin,
    const std::string& resource_id,
    google_apis::GDataErrorCode error,
    int64 largest_changestamp) {
  if (error != google_apis::HTTP_SUCCESS) {
    pending_batch_sync_origins_.insert(std::make_pair(origin, resource_id));
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
    return;
  }

  if (metadata_store_->incremental_sync_origins().empty()) {
    largest_fetched_changestamp_ = largest_changestamp;
    metadata_store_->SetLargestChangeStamp(
        largest_changestamp,
        base::Bind(&EmptyStatusCallback));
  }

  api_util_->ListFiles(
      resource_id,
      base::Bind(&DriveFileSyncService::DidGetDirectoryContentForBatchSync,
                 AsWeakPtr(),
                 callback,
                 origin,
                 resource_id,
                 largest_changestamp));
}

void DriveFileSyncService::DidGetDirectoryContentForBatchSync(
    const SyncStatusCallback& callback,
    const GURL& origin,
    const std::string& resource_id,
    int64 largest_changestamp,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> feed) {
  if (error != google_apis::HTTP_SUCCESS) {
    pending_batch_sync_origins_.insert(std::make_pair(origin, resource_id));
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
    return;
  }

  typedef ScopedVector<google_apis::ResourceEntry>::const_iterator iterator;
  for (iterator itr = feed->entries().begin();
       itr != feed->entries().end(); ++itr) {
    const google_apis::ResourceEntry& entry = **itr;
    if (entry.deleted())
      continue;

    SyncFileType file_type = SYNC_FILE_TYPE_UNKNOWN;
    if (entry.is_file())
      file_type = SYNC_FILE_TYPE_FILE;
    else if (entry.is_folder() && IsSyncFSDirectoryOperationEnabled())
      file_type = SYNC_FILE_TYPE_DIRECTORY;
    else
      continue;

    // Save to be fetched file to DB for restore in case of crash.
    DriveMetadata metadata;
    metadata.set_resource_id(entry.resource_id());
    metadata.set_md5_checksum(std::string());
    metadata.set_conflicted(false);
    metadata.set_to_be_fetched(true);

    base::FilePath path = TitleToPath(entry.title());
    fileapi::FileSystemURL url(CreateSyncableFileSystemURL(
        origin, path));
    // TODO(calvinlo): Write metadata and origin data as single batch command
    // so it's not possible for the DB to contain a DriveMetadata with an
    // unknown origin.
    metadata_store_->UpdateEntry(url, metadata,
                                 base::Bind(&EmptyStatusCallback));

    AppendFetchChange(origin, path, entry.resource_id(), file_type);
  }

  GURL next_feed_url;
  if (feed->GetNextFeedURL(&next_feed_url)) {
    api_util_->ContinueListing(
        next_feed_url,
        base::Bind(&DriveFileSyncService::DidGetDirectoryContentForBatchSync,
                   AsWeakPtr(),
                   callback,
                   origin,
                   resource_id,
                   largest_changestamp));
    return;
  }

  metadata_store_->AddIncrementalSyncOrigin(origin, resource_id);
  may_have_unfetched_changes_ = true;
  callback.Run(SYNC_STATUS_OK);
}

// TODO(tzik): Factor out this conflict resolution function.
ConflictResolutionResult DriveFileSyncService::ResolveConflictForLocalSync(
    SyncFileType local_file_type,
    const base::Time& local_updated_time,
    SyncFileType remote_file_type,
    const base::Time& remote_updated_time) {
  // Currently we always prioritize directories over files regardless of
  // conflict resolution policy.
  if (remote_file_type == SYNC_FILE_TYPE_DIRECTORY)
    return CONFLICT_RESOLUTION_REMOTE_WIN;

  if (conflict_resolution_ == CONFLICT_RESOLUTION_MANUAL)
    return CONFLICT_RESOLUTION_MARK_CONFLICT;

  DCHECK_EQ(CONFLICT_RESOLUTION_LAST_WRITE_WIN, conflict_resolution_);
  if (local_updated_time >= remote_updated_time ||
      remote_file_type == SYNC_FILE_TYPE_UNKNOWN) {
    return CONFLICT_RESOLUTION_LOCAL_WIN;
  }

  return CONFLICT_RESOLUTION_REMOTE_WIN;
}

void DriveFileSyncService::DidApplyLocalChange(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  running_local_sync_task_.reset();
  callback.Run(status);
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

  SyncOperationType operation =
      RemoteSyncOperationResolver::Resolve(remote_file_change,
                                           local_changes,
                                           param->local_metadata.file_type,
                                           param->drive_metadata.conflicted());

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "ProcessRemoteChange for %s %s%sremote_change: %s ==> %s",
            url.DebugString().c_str(),
            param->drive_metadata.conflicted() ? " (conflicted)" : " ",
            missing_local_file ? " (missing local file)" : " ",
            remote_file_change.DebugString().c_str(),
            SyncOperationTypeToString(operation));
  DCHECK_NE(SYNC_OPERATION_FAIL, operation);

  switch (operation) {
    case SYNC_OPERATION_ADD_FILE:
    case SYNC_OPERATION_ADD_DIRECTORY:
      param->sync_action = SYNC_ACTION_ADDED;
      break;
    case SYNC_OPERATION_UPDATE_FILE:
      param->sync_action = SYNC_ACTION_UPDATED;
      break;
    case SYNC_OPERATION_DELETE:
      param->sync_action = SYNC_ACTION_DELETED;
      break;
    case SYNC_OPERATION_NONE:
    case SYNC_OPERATION_DELETE_METADATA:
      param->sync_action = SYNC_ACTION_NONE;
      break;
    default:
      break;
  }

  switch (operation) {
    case SYNC_OPERATION_ADD_FILE:
    case SYNC_OPERATION_UPDATE_FILE:
      DownloadForRemoteSync(param.Pass());
      return;
    case SYNC_OPERATION_ADD_DIRECTORY:
    case SYNC_OPERATION_DELETE: {
      const FileChange& file_change = remote_file_change;
      remote_change_processor_->ApplyRemoteChange(
          file_change, base::FilePath(), url,
          base::Bind(&DriveFileSyncService::DidApplyRemoteChange, AsWeakPtr(),
                     base::Passed(&param)));
      return;
    }
    case SYNC_OPERATION_NONE:
      CompleteRemoteSync(param.Pass(), SYNC_STATUS_OK);
      return;
    case SYNC_OPERATION_CONFLICT:
      HandleConflictForRemoteSync(param.Pass(), base::Time(),
                                  remote_file_change.file_type(),
                                  SYNC_STATUS_OK);
      return;
    case SYNC_OPERATION_RESOLVE_TO_LOCAL:
      ResolveConflictToLocalForRemoteSync(param.Pass());
      return;
    case SYNC_OPERATION_RESOLVE_TO_REMOTE: {
      const FileSystemURL& url = param->remote_change.url;
      param->drive_metadata.set_conflicted(false);
      param->drive_metadata.set_to_be_fetched(true);
      metadata_store_->UpdateEntry(
          url, drive_metadata, base::Bind(&EmptyStatusCallback));
      param->sync_action = SYNC_ACTION_ADDED;
      if (param->remote_change.change.file_type() == SYNC_FILE_TYPE_FILE) {
        DownloadForRemoteSync(param.Pass());
        return;
      }

      // |remote_change_processor| should replace any existing file or directory
      // on ApplyRemoteChange call.
      const FileChange& file_change = remote_file_change;
      remote_change_processor_->ApplyRemoteChange(
          file_change, base::FilePath(), url,
          base::Bind(&DriveFileSyncService::DidApplyRemoteChange, AsWeakPtr(),
                     base::Passed(&param)));
      return;
    }
    case SYNC_OPERATION_DELETE_METADATA:
      if (missing_db_entry)
        CompleteRemoteSync(param.Pass(), SYNC_STATUS_OK);
      else
        DeleteMetadataForRemoteSync(param.Pass());
      return;
    case SYNC_OPERATION_FAIL:
      AbortRemoteSync(param.Pass(), SYNC_STATUS_FAILED);
      return;
  }
  NOTREACHED();
  AbortRemoteSync(param.Pass(), SYNC_STATUS_FAILED);
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
  webkit_blob::ScopedFile* temporary_file = &param->temporary_file;
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateTemporaryFile, temporary_file_dir_, temporary_file),
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

  const base::FilePath& temporary_file_path = param->temporary_file.path();
  std::string resource_id = param->remote_change.resource_id;
  DCHECK(!temporary_file_path.empty());

  // We should not use the md5 in metadata for FETCH type to avoid the download
  // finishes due to NOT_MODIFIED.
  std::string md5_checksum;
  if (!param->drive_metadata.to_be_fetched())
    md5_checksum = param->drive_metadata.md5_checksum();
  api_util_->DownloadFile(
      resource_id,
      md5_checksum,
      temporary_file_path,
      base::Bind(&DriveFileSyncService::DidDownloadFileForRemoteSync,
                 AsWeakPtr(),
                 base::Passed(&param)));
}

void DriveFileSyncService::DidDownloadFileForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    google_apis::GDataErrorCode error,
    const std::string& md5_checksum,
    int64 file_size,
    const base::Time& updated_time) {
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
  const base::FilePath& temporary_file_path = param->temporary_file.path();
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
  if (param->remote_change.change.IsFile()) {
    param->drive_metadata.set_type(DriveMetadata::RESOURCE_TYPE_FILE);
  } else {
    DCHECK(IsSyncFSDirectoryOperationEnabled());
    param->drive_metadata.set_type(DriveMetadata::RESOURCE_TYPE_FOLDER);
  }

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
  int64 changestamp = param->remote_change.changestamp;
  if (changestamp > 0) {
    DCHECK(metadata_store_->IsIncrementalSyncOrigin(origin));
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

  param->callback.Run(status, param->remote_change.url);
  if (status == SYNC_STATUS_OK && param->sync_action != SYNC_ACTION_NONE) {
    NotifyObserversFileStatusChanged(param->remote_change.url,
                                     SYNC_FILE_STATUS_SYNCED,
                                     param->sync_action,
                                     SYNC_DIRECTION_REMOTE_TO_LOCAL);
  }
}

void DriveFileSyncService::HandleConflictForRemoteSync(
    scoped_ptr<ProcessRemoteChangeParam> param,
    const base::Time& remote_updated_time,
    SyncFileType remote_file_type,
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
    const std::string& resource_id = param->remote_change.resource_id;
    api_util_->GetResourceEntry(
        resource_id,
        base::Bind(
            &DriveFileSyncService::DidGetRemoteFileMetadataForRemoteUpdatedTime,
            AsWeakPtr(),
            base::Bind(&DriveFileSyncService::HandleConflictForRemoteSync,
                       AsWeakPtr(),
                       base::Passed(&param))));
    return;
  }
  if (local_metadata.last_modified >= param->remote_change.updated_time) {
    // Local win case.
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "Resolving conflict for remote sync: %s: LOCAL WIN",
              url.DebugString().c_str());
    ResolveConflictToLocalForRemoteSync(param.Pass());
    return;
  }
  // Remote win case.
  // Make sure we reset the conflict flag and start over the remote sync
  // with empty local changes.
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Resolving conflict for remote sync: %s: REMOTE WIN",
            url.DebugString().c_str());
  drive_metadata.set_conflicted(false);
  drive_metadata.set_to_be_fetched(false);
  drive_metadata.set_type(
      SyncFileTypeToDriveMetadataResourceType(remote_file_type));
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

  // Re-add a fake local change to resolve it later in next LocalSync.
  SyncFileType local_file_type = param->local_metadata.file_type;
  remote_change_processor_->RecordFakeLocalChange(
      url,
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, local_file_type),
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
    int64 changestamp) {
  base::FilePath path = TitleToPath(entry.title());

  if (!entry.is_folder() && !entry.is_file() && !entry.deleted())
    return false;

  if (entry.is_folder() && !IsSyncFSDirectoryOperationEnabled())
    return false;

  SyncFileType file_type = entry.is_file() ?
      SYNC_FILE_TYPE_FILE : SYNC_FILE_TYPE_DIRECTORY;

  return AppendRemoteChangeInternal(
      origin, path, entry.deleted(),
      entry.resource_id(), changestamp,
      entry.deleted() ? std::string() : entry.file_md5(),
      entry.updated_time(), file_type);
}

bool DriveFileSyncService::AppendFetchChange(
    const GURL& origin,
    const base::FilePath& path,
    const std::string& resource_id,
    SyncFileType type) {
  return AppendRemoteChangeInternal(
      origin, path,
      false,  // is_deleted
      resource_id,
      0,  // changestamp
      std::string(),  // remote_file_md5
      base::Time(),  // updated_time
      type);
}

bool DriveFileSyncService::AppendRemoteChangeInternal(
    const GURL& origin,
    const base::FilePath& path,
    bool is_deleted,
    const std::string& remote_resource_id,
    int64 changestamp,
    const std::string& remote_file_md5,
    const base::Time& updated_time,
    SyncFileType file_type) {
  fileapi::FileSystemURL url(CreateSyncableFileSystemURL(origin, path));
  DCHECK(url.is_valid());

  // Note that we create a normalized path from url.path() rather than
  // path here (as FileSystemURL does extra normalization).
  base::FilePath::StringType normalized_path =
    fileapi::VirtualPath::GetNormalizedFilePath(url.path());

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

  RemoteChangeHandler::RemoteChange pending_change;
  if (remote_change_handler_.GetChangeForURL(url, &pending_change)) {
    if (pending_change.changestamp >= changestamp)
      return false;

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

  if (is_deleted) {
    // Drop any change if the change is for deletion and local resource id is
    // empty.
    if (local_resource_id.empty())
      return false;

    // Determine a file type of the deleted change by local metadata.
    if (!remote_resource_id.empty() &&
        !local_resource_id.empty() &&
        remote_resource_id == local_resource_id) {
      DCHECK(IsSyncFSDirectoryOperationEnabled() ||
             DriveMetadata::RESOURCE_TYPE_FILE == metadata.type());
      file_type = metadata.type() == DriveMetadata::RESOURCE_TYPE_FILE ?
          SYNC_FILE_TYPE_FILE : SYNC_FILE_TYPE_DIRECTORY;
    }

    if (has_db_entry) {
      metadata.set_resource_id(std::string());
      metadata_store_->UpdateEntry(url, metadata,
                                   base::Bind(&EmptyStatusCallback));
    }
  }

  FileChange file_change(is_deleted ? FileChange::FILE_CHANGE_DELETE
                                    : FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                         file_type);

  RemoteChangeHandler::RemoteChange remote_change(
      changestamp, remote_resource_id, remote_file_md5,
      updated_time, url, file_change);
  remote_change_handler_.AppendChange(remote_change);

  DVLOG(3) << "Append remote change: " << path.value()
           << " (" << normalized_path << ")"
           << "@" << changestamp << " "
           << file_change.DebugString();

  return true;
}

void DriveFileSyncService::RemoveRemoteChange(
    const FileSystemURL& url) {
  remote_change_handler_.RemoveChangeForURL(url);
}

void DriveFileSyncService::MarkConflict(
    const fileapi::FileSystemURL& url,
    DriveMetadata* drive_metadata,
    const SyncStatusCallback& callback) {
  DCHECK(drive_metadata);
  if (drive_metadata->resource_id().empty()) {
    // If the file does not have valid drive_metadata in the metadata store
    // we must have a pending remote change entry.
    RemoteChangeHandler::RemoteChange remote_change;
    const bool has_remote_change =
        remote_change_handler_.GetChangeForURL(url, &remote_change);
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
    callback.Run(base::Time::FromDoubleT(0.0),
                 SYNC_FILE_TYPE_UNKNOWN, SYNC_STATUS_OK);
    return;
  }

  SyncFileType file_type = SYNC_FILE_TYPE_UNKNOWN;
  if (entry->is_file())
    file_type = SYNC_FILE_TYPE_FILE;
  if (entry->is_folder())
    file_type = SYNC_FILE_TYPE_DIRECTORY;

  // If |file_type| is unknown, just use the other (local) version.
  callback.Run(entry->updated_time(), file_type, status);
}

SyncStatusCode DriveFileSyncService::GDataErrorCodeToSyncStatusCodeWrapper(
    google_apis::GDataErrorCode error) {
  last_gdata_error_ = error;
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK && !api_util_->IsAuthenticated())
    return SYNC_STATUS_AUTHENTICATION_FAILED;
  return status;
}

void DriveFileSyncService::MaybeStartFetchChanges() {
  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  // If we have pending_batch_sync_origins, try starting the batch sync.
  if (!pending_batch_sync_origins_.empty()) {
    if (GetCurrentState() == REMOTE_SERVICE_OK || may_have_unfetched_changes_) {
      task_manager_->ScheduleTaskIfIdle(
          base::Bind(&DriveFileSyncService::StartBatchSync, AsWeakPtr()));
    }
    return;
  }

  if (may_have_unfetched_changes_ &&
      !metadata_store_->incremental_sync_origins().empty()) {
    task_manager_->ScheduleTaskIfIdle(
        base::Bind(&DriveFileSyncService::FetchChangesForIncrementalSync,
                   AsWeakPtr()));
  }
}

void DriveFileSyncService::OnNotificationReceived() {
  util::Log(logging::LOG_INFO,
            FROM_HERE,
            "Notification received to check for Google Drive updates");

  // Likely indicating the network is enabled again.
  UpdateServiceState(REMOTE_SERVICE_OK, "Got push notification for Drive.");

  // TODO(calvinlo): Try to eliminate may_have_unfetched_changes_ variable.
  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

void DriveFileSyncService::OnPushNotificationEnabled(bool enabled) {
  const char* status = (enabled ? "enabled" : "disabled");
  util::Log(logging::LOG_INFO,
            FROM_HERE,
            "XMPP Push notification is %s", status);
}

void DriveFileSyncService::MaybeScheduleNextTask() {
  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  // Notify observer of the update of |pending_changes_|.
  FOR_EACH_OBSERVER(Observer, service_observers_,
                    OnRemoteChangeQueueUpdated(
                        remote_change_handler_.ChangesSize()));

  MaybeStartFetchChanges();
}

void DriveFileSyncService::NotifyLastOperationStatus(
    SyncStatusCode sync_status) {
  UpdateServiceStateFromLastOperationStatus(sync_status, last_gdata_error_);
}

// static
std::string DriveFileSyncService::PathToTitle(const base::FilePath& path) {
  if (!IsSyncFSDirectoryOperationEnabled())
    return path.AsUTF8Unsafe();

  return fileapi::FilePathToString(
      base::FilePath(fileapi::VirtualPath::GetNormalizedFilePath(path)));
}

// static
base::FilePath DriveFileSyncService::TitleToPath(const std::string& title) {
  if (!IsSyncFSDirectoryOperationEnabled())
    return base::FilePath::FromUTF8Unsafe(title);

  return fileapi::StringToFilePath(title).NormalizePathSeparators();
}

// static
DriveMetadata::ResourceType
DriveFileSyncService::SyncFileTypeToDriveMetadataResourceType(
    SyncFileType file_type) {
  DCHECK_NE(SYNC_FILE_TYPE_UNKNOWN, file_type);
  switch (file_type) {
    case SYNC_FILE_TYPE_UNKNOWN:
      return DriveMetadata_ResourceType_RESOURCE_TYPE_FILE;
    case SYNC_FILE_TYPE_FILE:
      return DriveMetadata_ResourceType_RESOURCE_TYPE_FILE;
    case SYNC_FILE_TYPE_DIRECTORY:
      return DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER;
  }
  NOTREACHED();
  return DriveMetadata_ResourceType_RESOURCE_TYPE_FILE;
}

// static
SyncFileType DriveFileSyncService::DriveMetadataResourceTypeToSyncFileType(
    DriveMetadata::ResourceType resource_type) {
  switch (resource_type) {
    case DriveMetadata_ResourceType_RESOURCE_TYPE_FILE:
      return SYNC_FILE_TYPE_FILE;
    case DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER:
      return SYNC_FILE_TYPE_DIRECTORY;
  }
  NOTREACHED();
  return SYNC_FILE_TYPE_UNKNOWN;
}

void DriveFileSyncService::FetchChangesForIncrementalSync(
    const SyncStatusCallback& callback) {
  DCHECK(may_have_unfetched_changes_);
  DCHECK(pending_batch_sync_origins_.empty());
  DCHECK(!metadata_store_->incremental_sync_origins().empty());

  DVLOG(1) << "FetchChangesForIncrementalSync (start_changestamp:"
           << (largest_fetched_changestamp_ + 1) << ")";

  api_util_->ListChanges(
      largest_fetched_changestamp_ + 1,
      base::Bind(&DriveFileSyncService::DidFetchChangesForIncrementalSync,
                 AsWeakPtr(),
                 callback,
                 false));

  may_have_unfetched_changes_ = false;
}

void DriveFileSyncService::DidFetchChangesForIncrementalSync(
    const SyncStatusCallback& callback,
    bool has_new_changes,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> changes) {
  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
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
    has_new_changes = AppendRemoteChange(
        origin, entry, entry.changestamp()) || has_new_changes;
  }

  if (reset_sync_root) {
    util::Log(logging::LOG_WARNING,
              FROM_HERE,
              "Detected unexpected SyncRoot deletion.");
    metadata_store_->SetSyncRootDirectory(std::string());
  }
  for (std::set<GURL>::iterator itr = reset_origins.begin();
       itr != reset_origins.end(); ++itr) {
    util::Log(logging::LOG_WARNING,
              FROM_HERE,
              "Detected unexpected OriginRoot deletion: %s",
              itr->spec().c_str());
    pending_batch_sync_origins_.erase(*itr);
    metadata_store_->SetOriginRootDirectory(*itr, std::string());
  }

  GURL next_feed;
  if (changes->GetNextFeedURL(&next_feed))
    may_have_unfetched_changes_ = true;

  if (!changes->entries().empty())
    largest_fetched_changestamp_ = changes->entries().back()->changestamp();

  callback.Run(SYNC_STATUS_OK);
}

bool DriveFileSyncService::GetOriginForEntry(
    const google_apis::ResourceEntry& entry,
    GURL* origin_out) {
  typedef ScopedVector<google_apis::Link>::const_iterator iterator;
  for (iterator itr = entry.links().begin();
       itr != entry.links().end(); ++itr) {
    if ((*itr)->type() != google_apis::Link::LINK_PARENT)
      continue;

    std::string resource_id(
        ::drive::util::ExtractResourceIdFromUrl((*itr)->href()));
    if (resource_id.empty())
      continue;

    GURL origin;
    metadata_store_->GetOriginByOriginRootDirectoryId(resource_id, &origin);
    if (!origin.is_valid() || !metadata_store_->IsIncrementalSyncOrigin(origin))
      continue;

    *origin_out = origin;
    return true;
  }
  return false;
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

  api_util_->GetDriveDirectoryForSyncRoot(base::Bind(
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

  api_util_->GetDriveDirectoryForOrigin(
      sync_root_resource_id,
      origin,
      base::Bind(&DriveFileSyncService::DidEnsureOriginRoot,
                 AsWeakPtr(),
                 origin,
                 callback));
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
  }
  callback.Run(status, resource_id);
}

std::string DriveFileSyncService::sync_root_resource_id() {
  return metadata_store_->sync_root_directory();
}

}  // namespace sync_file_system

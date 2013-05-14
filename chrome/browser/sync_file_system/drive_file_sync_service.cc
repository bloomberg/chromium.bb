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
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google_apis/drive_notification_manager.h"
#include "chrome/browser/google_apis/drive_notification_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_file_sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/remote_change_handler.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/remote_sync_operation_resolver.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "webkit/blob/scoped_file.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_file_type.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

const base::FilePath::CharType kTempDirName[] = FILE_PATH_LITERAL("tmp");
const base::FilePath::CharType kSyncFileSystemDir[] =
    FILE_PATH_LITERAL("Sync FileSystem");
const base::FilePath::CharType kSyncFileSystemDirDev[] =
    FILE_PATH_LITERAL("Sync FileSystem Dev");

const base::FilePath::CharType* GetSyncFileSystemDir() {
  return IsSyncDirectoryOperationEnabled()
      ? kSyncFileSystemDirDev : kSyncFileSystemDir;
}

bool CreateTemporaryFile(const base::FilePath& dir_path,
                         webkit_blob::ScopedFile* temp_file) {
  base::FilePath temp_file_path;
  const bool success = file_util::CreateDirectory(dir_path) &&
      file_util::CreateTemporaryFileInDir(dir_path, &temp_file_path);
  if (!success)
    return success;
  *temp_file = webkit_blob::ScopedFile(
      temp_file_path,
      webkit_blob::ScopedFile::DELETE_ON_SCOPE_OUT,
      base::MessageLoopProxy::current());
  return success;
}

void EmptyStatusCallback(SyncStatusCode status) {}

std::string PathToTitle(const base::FilePath& path) {
  if (!IsSyncDirectoryOperationEnabled())
    return path.AsUTF8Unsafe();

  return fileapi::FilePathToString(
      base::FilePath(fileapi::VirtualPath::GetNormalizedFilePath(path)));
}

base::FilePath TitleToPath(const std::string& title) {
  if (!IsSyncDirectoryOperationEnabled())
    return base::FilePath::FromUTF8Unsafe(title);

  return fileapi::StringToFilePath(title).NormalizePathSeparators();
}

DriveMetadata::ResourceType SyncFileTypeToDriveMetadataResourceType(
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

void SyncFileCallbackAdapter(
    const SyncStatusCallback& status_callback,
    const SyncFileCallback& callback,
    SyncStatusCode status,
    const FileSystemURL& url) {
  status_callback.Run(status);
  callback.Run(status, url);
}

}  // namespace

const char DriveFileSyncService::kServiceName[] = "syncfs";
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

struct DriveFileSyncService::ApplyLocalChangeParam {
  FileSystemURL url;
  FileChange local_change;
  base::FilePath local_path;
  SyncFileMetadata local_metadata;
  DriveMetadata drive_metadata;
  bool has_drive_metadata;
  SyncStatusCallback callback;

  ApplyLocalChangeParam(const FileSystemURL& url,
                        const FileChange& local_change,
                        const base::FilePath& local_path,
                        const SyncFileMetadata& local_metadata,
                        const SyncStatusCallback& callback)
      : url(url),
        local_change(local_change),
        local_path(local_path),
        local_metadata(local_metadata),
        has_drive_metadata(false),
        callback(callback) {}
};

// DriveFileSyncService ------------------------------------------------------

DriveFileSyncService::~DriveFileSyncService() {
  if (sync_client_)
    sync_client_->RemoveObserver(this);

  google_apis::DriveNotificationManager* drive_notification_manager =
      google_apis::DriveNotificationManagerFactory::GetForProfile(profile_);
  if (drive_notification_manager)
    drive_notification_manager->RemoveObserver(this);
}

scoped_ptr<DriveFileSyncService> DriveFileSyncService::Create(
    Profile* profile) {
  scoped_ptr<DriveFileSyncService> service(new DriveFileSyncService(profile));
  scoped_ptr<DriveFileSyncTaskManager> task_manager(
      new DriveFileSyncTaskManager(service->AsWeakPtr()));
  SyncStatusCallback callback = base::Bind(
      &DriveFileSyncTaskManager::Initialize, task_manager->AsWeakPtr());
  service->Initialize(task_manager.Pass(), callback);
  return service.Pass();
}

scoped_ptr<DriveFileSyncService> DriveFileSyncService::CreateForTesting(
    Profile* profile,
    const base::FilePath& base_dir,
    scoped_ptr<DriveFileSyncClientInterface> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store) {
  scoped_ptr<DriveFileSyncService> service(new DriveFileSyncService(profile));
  scoped_ptr<DriveFileSyncTaskManager> task_manager(
      new DriveFileSyncTaskManager(service->AsWeakPtr()));
  SyncStatusCallback callback = base::Bind(
      &DriveFileSyncTaskManager::Initialize, task_manager->AsWeakPtr());
  service->InitializeForTesting(
      task_manager.Pass(), base_dir, sync_client.Pass(),
      metadata_store.Pass(), callback);
  return service.Pass();
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
  DVLOG(1) << "OnAuthenticated";

  UpdateServiceState(REMOTE_SERVICE_OK, "Authenticated");

  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

void DriveFileSyncService::OnNetworkConnected() {
  if (state_ == REMOTE_SERVICE_OK)
    return;
  DVLOG(1) << "OnNetworkConnected";

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
      conflict_resolution_(kDefaultPolicy) {
}

void DriveFileSyncService::Initialize(
    scoped_ptr<DriveFileSyncTaskManager> task_manager,
    const SyncStatusCallback& callback) {
  DCHECK(profile_);
  DCHECK(!metadata_store_);
  DCHECK(!task_manager_);

  task_manager_ = task_manager.Pass();

  temporary_file_dir_ =
      profile_->GetPath().Append(GetSyncFileSystemDir()).Append(kTempDirName);

  sync_client_.reset(new DriveFileSyncClient(profile_));
  sync_client_->AddObserver(this);

  metadata_store_.reset(new DriveMetadataStore(
      profile_->GetPath().Append(GetSyncFileSystemDir()),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE)));

  metadata_store_->Initialize(
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore,
                 AsWeakPtr(), callback));
}

void DriveFileSyncService::InitializeForTesting(
    scoped_ptr<DriveFileSyncTaskManager> task_manager,
    const base::FilePath& base_dir,
    scoped_ptr<DriveFileSyncClientInterface> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store,
    const SyncStatusCallback& callback) {
  DCHECK(!metadata_store_);
  DCHECK(!task_manager_);

  task_manager_ = task_manager.Pass();
  temporary_file_dir_ = base_dir.Append(kTempDirName);

  sync_client_ = sync_client.Pass();
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

  // Mark all the batch sync origins as 'pending' so that we can start
  // batch sync when we're ready.
  for (std::map<GURL, std::string>::const_iterator itr =
           metadata_store_->batch_sync_origins().begin();
       itr != metadata_store_->batch_sync_origins().end();
       ++itr) {
    pending_batch_sync_origins_.insert(itr->first);
  }

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
    AppendFetchChange(url.origin(), url.path(), resource_id, file_type);
  }

  if (!sync_root_resource_id().empty())
    sync_client_->EnsureSyncRootIsNotInMyDrive(sync_root_resource_id());

  callback.Run(status);
  may_have_unfetched_changes_ = true;

  google_apis::DriveNotificationManager* drive_notification_manager =
      google_apis::DriveNotificationManagerFactory::GetForProfile(profile_);
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
    DVLOG(1) << "Service state changed: " << state << ": " << description;
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
  if (!metadata_store_->IsOriginDisabled(origin)) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  metadata_store_->EnableOrigin(origin, callback);
  pending_batch_sync_origins_.insert(origin);
}

void DriveFileSyncService::DoDisableOriginForTrackingChanges(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  if (!metadata_store_->IsBatchSyncOrigin(origin) &&
      !metadata_store_->IsIncrementalSyncOrigin(origin)) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  remote_change_handler_.RemoveChangesForOrigin(origin);
  pending_batch_sync_origins_.erase(origin);
  metadata_store_->DisableOrigin(origin, callback);
}

void DriveFileSyncService::DoUninstallOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  std::string resource_id = metadata_store_->GetResourceIdForOrigin(origin);

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
  sync_client_->DeleteFile(
      resource_id,
      std::string(),
      base::Bind(&DriveFileSyncService::DidUninstallOrigin,
                 AsWeakPtr(), origin, callback));
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

  DVLOG(1) << "ProcessRemoteChange for " << remote_change.url.DebugString()
           << " remote_change:" << remote_change.change.DebugString();

  scoped_ptr<ProcessRemoteChangeParam> param(new ProcessRemoteChangeParam(
      remote_change, callback));
  remote_change_processor_->PrepareForProcessRemoteChange(
      remote_change.url,
      kServiceName,
      base::Bind(&DriveFileSyncService::DidPrepareForProcessRemoteChange,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DoApplyLocalChange(
    const FileChange& local_file_change,
    const base::FilePath& local_file_path,
    const SyncFileMetadata& local_file_metadata,
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  // TODO(nhiroki): support directory operations (http://crbug.com/161442).
  DCHECK(IsSyncDirectoryOperationEnabled() ||
         !local_file_change.IsDirectory());

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED) {
    callback.Run(SYNC_STATUS_SYNC_DISABLED);
    return;
  }

  if (!metadata_store_->IsIncrementalSyncOrigin(url.origin()) &&
      !metadata_store_->IsBatchSyncOrigin(url.origin())) {
    // We may get called by LocalFileSyncService to sync local changes
    // for the origins that are disabled.
    DVLOG(1) << "Got request for stray origin: " << url.origin().spec();
    callback.Run(SYNC_STATUS_UNKNOWN_ORIGIN);
    return;
  }

  DriveMetadata metadata;
  const bool has_metadata =
      (metadata_store_->ReadEntry(url, &metadata) == SYNC_STATUS_OK);

  scoped_ptr<ApplyLocalChangeParam> param(new ApplyLocalChangeParam(
      url, local_file_change, local_file_path, local_file_metadata, callback));
  param->has_drive_metadata = has_metadata;
  param->drive_metadata = metadata;
  if (!has_metadata)
    param->drive_metadata.set_md5_checksum(std::string());

  EnsureOriginRootDirectory(
      url.origin(),
      base::Bind(&DriveFileSyncService::ApplyLocalChangeInternal,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::UpdateRegisteredOrigins() {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!extension_service)
    return;

  std::vector<GURL> origins;
  metadata_store_->GetAllOrigins(&origins);
  for (std::vector<GURL>::const_iterator itr = origins.begin();
       itr != origins.end(); ++itr) {
    std::string extension_id = itr->host();
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(extension_id);

    if (!extension_service->GetInstalledExtension(extension_id)) {
      // Extension has been uninstalled.
      UninstallOrigin(origin, base::Bind(&EmptyStatusCallback));
    } else if ((metadata_store_->IsBatchSyncOrigin(origin) ||
                metadata_store_->IsIncrementalSyncOrigin(origin)) &&
               !extension_service->IsExtensionEnabled(extension_id)) {
      // Extension has been disabled.
      metadata_store_->DisableOrigin(origin, base::Bind(&EmptyStatusCallback));
    } else if (metadata_store_->IsOriginDisabled(origin) &&
               extension_service->IsExtensionEnabled(extension_id)) {
      // Extension has been re-enabled.
      metadata_store_->EnableOrigin(origin, base::Bind(&EmptyStatusCallback));
      pending_batch_sync_origins_.insert(origin);
    }
  }
}

void DriveFileSyncService::StartBatchSync(
    const SyncStatusCallback& callback) {
  DCHECK(GetCurrentState() == REMOTE_SERVICE_OK || may_have_unfetched_changes_);

  if (pending_batch_sync_origins_.empty()) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  GURL origin = *pending_batch_sync_origins_.begin();
  pending_batch_sync_origins_.erase(pending_batch_sync_origins_.begin());
  std::string resource_id = metadata_store_->GetResourceIdForOrigin(origin);
  DCHECK(!resource_id.empty());

  DCHECK(!metadata_store_->IsOriginDisabled(origin));

  DVLOG(1) << "Start batch sync for:" << origin.spec();

  sync_client_->GetLargestChangeStamp(
      base::Bind(&DriveFileSyncService::DidGetLargestChangeStampForBatchSync,
                 AsWeakPtr(), callback, origin, resource_id));

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

  // Add this origin to batch sync origin if it hasn't been already.
  if (!metadata_store_->IsKnownOrigin(origin)) {
    metadata_store_->AddBatchSyncOrigin(origin, resource_id);
    pending_batch_sync_origins_.insert(origin);
  }

  callback.Run(SYNC_STATUS_OK);
}

void DriveFileSyncService::DidUninstallOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  if (status != SYNC_STATUS_OK) {
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
    pending_batch_sync_origins_.insert(origin);
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
    return;
  }

  if (metadata_store_->incremental_sync_origins().empty()) {
    largest_fetched_changestamp_ = largest_changestamp;
    metadata_store_->SetLargestChangeStamp(
        largest_changestamp,
        base::Bind(&EmptyStatusCallback));
  }

  sync_client_->ListFiles(
      resource_id,
      base::Bind(
          &DriveFileSyncService::DidGetDirectoryContentForBatchSync,
          AsWeakPtr(), callback, origin, largest_changestamp));
}

void DriveFileSyncService::DidGetDirectoryContentForBatchSync(
    const SyncStatusCallback& callback,
    const GURL& origin,
    int64 largest_changestamp,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> feed) {
  if (error != google_apis::HTTP_SUCCESS) {
    pending_batch_sync_origins_.insert(origin);
    callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
    return;
  }

  typedef ScopedVector<google_apis::ResourceEntry>::const_iterator iterator;
  for (iterator itr = feed->entries().begin();
       itr != feed->entries().end(); ++itr) {
    AppendRemoteChange(origin, **itr, largest_changestamp,
                       RemoteChangeHandler::REMOTE_SYNC_TYPE_BATCH);
  }

  GURL next_feed_url;
  if (feed->GetNextFeedURL(&next_feed_url)) {
    sync_client_->ContinueListing(
        next_feed_url,
        base::Bind(
            &DriveFileSyncService::DidGetDirectoryContentForBatchSync,
            AsWeakPtr(), callback, origin, largest_changestamp));
    return;
  }

  // Move |origin| to the incremental sync origin set if the origin has no file.
  if (metadata_store_->IsBatchSyncOrigin(origin) &&
      !remote_change_handler_.HasChangesForOrigin(origin)) {
    metadata_store_->MoveBatchSyncOriginToIncremental(origin);
  }

  may_have_unfetched_changes_ = true;
  callback.Run(SYNC_STATUS_OK);
}

void DriveFileSyncService::ApplyLocalChangeInternal(
    scoped_ptr<ApplyLocalChangeParam> param,
    SyncStatusCode status,
    const std::string& origin_resource_id) {
  if (status != SYNC_STATUS_OK) {
    param->callback.Run(status);
    return;
  }

  const FileSystemURL& url = param->url;
  const FileChange& local_file_change = param->local_change;
  const base::FilePath& local_file_path = param->local_path;
  DriveMetadata& drive_metadata = param->drive_metadata;
  const SyncStatusCallback& callback = param->callback;

  RemoteChangeHandler::RemoteChange remote_change;
  const bool has_remote_change =
      remote_change_handler_.GetChangeForURL(url, &remote_change);
  if (has_remote_change && param->drive_metadata.resource_id().empty())
    param->drive_metadata.set_resource_id(remote_change.resource_id);

  LocalSyncOperationType operation =
      LocalSyncOperationResolver::Resolve(
          local_file_change,
          has_remote_change ? &remote_change.change : NULL,
          param->has_drive_metadata ? &drive_metadata : NULL);

  DVLOG(1) << "ApplyLocalChange for " << url.DebugString()
           << " local_change:" << local_file_change.DebugString()
           << " ==> operation:" << operation;

  switch (operation) {
    case LOCAL_SYNC_OPERATION_ADD_FILE:
      sync_client_->UploadNewFile(
          origin_resource_id,
          local_file_path,
          PathToTitle(url.path()),
          base::Bind(&DriveFileSyncService::DidUploadNewFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    case LOCAL_SYNC_OPERATION_ADD_DIRECTORY:
      DCHECK(IsSyncDirectoryOperationEnabled());
      sync_client_->CreateDirectory(
          origin_resource_id,
          PathToTitle(url.path()),
          base::Bind(&DriveFileSyncService::DidCreateDirectoryForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    case LOCAL_SYNC_OPERATION_UPDATE_FILE:
      DCHECK(param->has_drive_metadata);
      sync_client_->UploadExistingFile(
          drive_metadata.resource_id(),
          drive_metadata.md5_checksum(),
          local_file_path,
          base::Bind(&DriveFileSyncService::DidUploadExistingFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    case LOCAL_SYNC_OPERATION_DELETE_FILE:
      DCHECK(param->has_drive_metadata);
      sync_client_->DeleteFile(
          drive_metadata.resource_id(),
          drive_metadata.md5_checksum(),
          base::Bind(&DriveFileSyncService::DidDeleteFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    case LOCAL_SYNC_OPERATION_DELETE_DIRECTORY:
      DCHECK(IsSyncDirectoryOperationEnabled());
      DCHECK(param->has_drive_metadata);
      // This does not handle recursive directory deletion
      // (which should not happen other than after a restart).
      sync_client_->DeleteFile(
          drive_metadata.resource_id(),
          std::string(),  // empty etag
          base::Bind(&DriveFileSyncService::DidDeleteFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    case LOCAL_SYNC_OPERATION_NONE:
      callback.Run(SYNC_STATUS_OK);
      return;
    case LOCAL_SYNC_OPERATION_CONFLICT:
      HandleConflictForLocalSync(param.Pass());
      return;
    case LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL:
      sync_client_->DeleteFile(
          drive_metadata.resource_id(),
          drive_metadata.md5_checksum(),
          base::Bind(
              &DriveFileSyncService::DidDeleteForResolveToLocalForLocalSync,
              AsWeakPtr(),
              origin_resource_id, local_file_path, url,
              base::Passed(&param)));
      return;
    case LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE:
      ResolveConflictToRemoteForLocalSync(
          param.Pass(),
          remote_change.change.file_type());
      return;
    case LOCAL_SYNC_OPERATION_DELETE_METADATA:
      metadata_store_->DeleteEntry(
          url,
          base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                     AsWeakPtr(), base::Passed(&param),
                     google_apis::HTTP_SUCCESS));
      return;
    case LOCAL_SYNC_OPERATION_FAIL: {
      callback.Run(SYNC_STATUS_FAILED);
      return;
    }
  }
  NOTREACHED();
  callback.Run(SYNC_STATUS_FAILED);
}

void DriveFileSyncService::DidDeleteForResolveToLocalForLocalSync(
    const std::string& origin_resource_id,
    const base::FilePath& local_file_path,
    const fileapi::FileSystemURL& url,
    scoped_ptr<ApplyLocalChangeParam> param,
    google_apis::GDataErrorCode error) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_NOT_FOUND) {
    RemoveRemoteChange(param->url);
    param->callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
    return;
  }

  DCHECK_NE(SYNC_FILE_TYPE_UNKNOWN, param->local_metadata.file_type);
  if (param->local_metadata.file_type == SYNC_FILE_TYPE_FILE) {
    sync_client_->UploadNewFile(
        origin_resource_id,
        local_file_path,
        PathToTitle(url.path()),
        base::Bind(&DriveFileSyncService::DidUploadNewFileForLocalSync,
                   AsWeakPtr(), base::Passed(&param)));
    return;
  }

  DCHECK(IsSyncDirectoryOperationEnabled());
  DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, param->local_metadata.file_type);
  sync_client_->CreateDirectory(
      origin_resource_id,
      PathToTitle(url.path()),
      base::Bind(&DriveFileSyncService::DidCreateDirectoryForLocalSync,
                 AsWeakPtr(), base::Passed(&param)));
}

void DriveFileSyncService::DidApplyLocalChange(
    scoped_ptr<ApplyLocalChangeParam> param,
    const google_apis::GDataErrorCode error,
    SyncStatusCode status) {
  if (status == SYNC_STATUS_OK) {
    RemoveRemoteChange(param->url);
    status = GDataErrorCodeToSyncStatusCodeWrapper(error);
  }
  param->callback.Run(status);
}

void DriveFileSyncService::DidResolveConflictToRemoteChange(
    scoped_ptr<ApplyLocalChangeParam> param,
    SyncStatusCode status) {
  DCHECK(param->has_drive_metadata);
  if (status != SYNC_STATUS_OK) {
    param->callback.Run(status);
    return;
  }

  SyncFileType file_type = SYNC_FILE_TYPE_FILE;
  if (param->drive_metadata.type() == DriveMetadata::RESOURCE_TYPE_FOLDER)
    file_type = SYNC_FILE_TYPE_DIRECTORY;
  AppendFetchChange(param->url.origin(), param->url.path(),
                    param->drive_metadata.resource_id(),
                    file_type);
  param->callback.Run(status);
}

void DriveFileSyncService::DidUploadNewFileForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    google_apis::GDataErrorCode error,
    const std::string& resource_id,
    const std::string& file_md5) {
  DCHECK(param);
  const FileSystemURL& url = param->url;
  switch (error) {
    case google_apis::HTTP_CREATED: {
      param->drive_metadata.set_resource_id(resource_id);
      param->drive_metadata.set_md5_checksum(file_md5);
      param->drive_metadata.set_conflicted(false);
      param->drive_metadata.set_to_be_fetched(false);
      param->drive_metadata.set_type(DriveMetadata::RESOURCE_TYPE_FILE);
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
    case google_apis::HTTP_CONFLICT:
      // File-file conflict is found.
      // Populates a fake drive_metadata and set has_drive_metadata = true.
      // In HandleConflictLocalSync:
      // - If conflict_resolution is manual, we'll change conflicted to true
      //   and save the metadata.
      // - Otherwise we'll save the metadata with empty md5 and will start
      //   over local sync as UploadExistingFile.
      param->drive_metadata.set_resource_id(resource_id);
      param->drive_metadata.set_md5_checksum(std::string());
      param->drive_metadata.set_conflicted(false);
      param->drive_metadata.set_to_be_fetched(false);
      param->drive_metadata.set_type(DriveMetadata::RESOURCE_TYPE_FILE);
      param->has_drive_metadata = true;
      HandleConflictForLocalSync(param.Pass());
      return;

    default:
      param->callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
  }
}

void DriveFileSyncService::DidCreateDirectoryForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  DCHECK(param);
  const FileSystemURL& url = param->url;
  switch (error) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED: {
      param->drive_metadata.set_resource_id(resource_id);
      param->drive_metadata.set_md5_checksum(std::string());
      param->drive_metadata.set_conflicted(false);
      param->drive_metadata.set_to_be_fetched(false);
      param->drive_metadata.set_type(DriveMetadata::RESOURCE_TYPE_FOLDER);
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

    case google_apis::HTTP_CONFLICT:
      // There were conflicts and a file was left.
      // TODO(kinuko): Handle the latter case (http://crbug.com/237090).
      // Fall-through

    default:
      param->callback.Run(GDataErrorCodeToSyncStatusCodeWrapper(error));
  }
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
      param->drive_metadata.set_type(DriveMetadata::RESOURCE_TYPE_FILE);
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
          PathToTitle(url.path()),
          base::Bind(&DriveFileSyncService::DidUploadNewFileForLocalSync,
                     AsWeakPtr(), base::Passed(&param)));
      return;
    }
    default: {
      const SyncStatusCode status =
          GDataErrorCodeToSyncStatusCodeWrapper(error);
      DCHECK_NE(SYNC_STATUS_OK, status);
      param->callback.Run(status);
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
      param->callback.Run(status);
      return;
    }
  }
}

void DriveFileSyncService::HandleConflictForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param) {
  DCHECK(param);
  DriveMetadata& drive_metadata = param->drive_metadata;
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
    SyncFileType remote_file_type,
    SyncStatusCode status) {
  DCHECK(param);
  const FileSystemURL& url = param->url;
  DriveMetadata& drive_metadata = param->drive_metadata;
  SyncFileMetadata& local_metadata = param->local_metadata;
  if (status != SYNC_STATUS_OK) {
    param->callback.Run(status);
    return;
  }

  // Currently we always prioritize directories over files regardless of
  // conflict resolution policy.
  DCHECK(param->local_change.IsFile());
  if (remote_file_type == SYNC_FILE_TYPE_DIRECTORY) {
    ResolveConflictToRemoteForLocalSync(param.Pass(), SYNC_FILE_TYPE_DIRECTORY);
    return;
  }

  if (conflict_resolution_ == CONFLICT_RESOLUTION_MANUAL) {
    if (drive_metadata.conflicted()) {
      // It's already conflicting; no need to update metadata.
      param->callback.Run(SYNC_STATUS_FAILED);
      return;
    }
    MarkConflict(url, &drive_metadata,
                 base::Bind(&DriveFileSyncService::DidApplyLocalChange,
                            AsWeakPtr(), base::Passed(&param),
                            google_apis::HTTP_CONFLICT));
    return;
  }

  DCHECK_EQ(CONFLICT_RESOLUTION_LAST_WRITE_WIN, conflict_resolution_);

  if (local_metadata.last_modified >= remote_updated_time ||
      remote_file_type == SYNC_FILE_TYPE_UNKNOWN) {
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
    drive_metadata.set_type(
        SyncFileTypeToDriveMetadataResourceType(SYNC_FILE_TYPE_FILE));
    metadata_store_->UpdateEntry(
        url, drive_metadata,
        base::Bind(&DriveFileSyncService::StartOverLocalSync, AsWeakPtr(),
                   base::Passed(&param)));
    return;
  }
  // Remote win case.
  DVLOG(1) << "Resolving conflict for local sync:"
           << url.DebugString() << ": REMOTE WIN";
  ResolveConflictToRemoteForLocalSync(param.Pass(), SYNC_FILE_TYPE_FILE);
}

void DriveFileSyncService::ResolveConflictToRemoteForLocalSync(
    scoped_ptr<ApplyLocalChangeParam> param,
    SyncFileType remote_file_type) {
  DCHECK(param);
  const FileSystemURL& url = param->url;
  DriveMetadata& drive_metadata = param->drive_metadata;
  // Mark the file as to-be-fetched.
  DCHECK(!drive_metadata.resource_id().empty());
  drive_metadata.set_conflicted(false);
  drive_metadata.set_to_be_fetched(true);
  drive_metadata.set_type(
      SyncFileTypeToDriveMetadataResourceType(remote_file_type));
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
    param->callback.Run(status);
    return;
  }
  RemoveRemoteChange(param->url);
  DoApplyLocalChange(
      param->local_change, param->local_path, param->local_metadata,
      param->url, param->callback);
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

  RemoteSyncOperationType operation =
      RemoteSyncOperationResolver::Resolve(remote_file_change,
                                           local_changes,
                                           param->local_metadata.file_type,
                                           param->drive_metadata.conflicted());

  DVLOG(1) << "ProcessRemoteChange for " << url.DebugString()
           << (param->drive_metadata.conflicted() ? " (conflicted)" : " ")
           << (missing_local_file ? " (missing local file)" : " ")
           << " remote_change: " << remote_file_change.DebugString()
           << " ==> operation: " << operation;
  DCHECK_NE(REMOTE_SYNC_OPERATION_FAIL, operation);

  switch (operation) {
    case REMOTE_SYNC_OPERATION_ADD_FILE:
    case REMOTE_SYNC_OPERATION_ADD_DIRECTORY:
      param->sync_action = SYNC_ACTION_ADDED;
      break;
    case REMOTE_SYNC_OPERATION_UPDATE_FILE:
      param->sync_action = SYNC_ACTION_UPDATED;
      break;
    case REMOTE_SYNC_OPERATION_DELETE_FILE:
    case REMOTE_SYNC_OPERATION_DELETE_DIRECTORY:
      param->sync_action = SYNC_ACTION_DELETED;
      break;
    case REMOTE_SYNC_OPERATION_NONE:
    case REMOTE_SYNC_OPERATION_DELETE_METADATA:
      param->sync_action = SYNC_ACTION_NONE;
      break;
    default:
      break;
  }

  switch (operation) {
    case REMOTE_SYNC_OPERATION_ADD_FILE:
    case REMOTE_SYNC_OPERATION_UPDATE_FILE:
      DownloadForRemoteSync(param.Pass());
      return;
    case REMOTE_SYNC_OPERATION_ADD_DIRECTORY:
    case REMOTE_SYNC_OPERATION_DELETE_DIRECTORY:
    case REMOTE_SYNC_OPERATION_DELETE_FILE: {
      const FileChange& file_change = remote_file_change;
      remote_change_processor_->ApplyRemoteChange(
          file_change, base::FilePath(), url,
          base::Bind(&DriveFileSyncService::DidApplyRemoteChange, AsWeakPtr(),
                     base::Passed(&param)));
      return;
    }
    case REMOTE_SYNC_OPERATION_NONE:
      CompleteRemoteSync(param.Pass(), SYNC_STATUS_OK);
      return;
    case REMOTE_SYNC_OPERATION_CONFLICT:
      HandleConflictForRemoteSync(param.Pass(), base::Time(),
                                  remote_file_change.file_type(),
                                  SYNC_STATUS_OK);
      return;
    case REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL:
      ResolveConflictToLocalForRemoteSync(param.Pass());
      return;
    case REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE: {
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
    case REMOTE_SYNC_OPERATION_DELETE_METADATA:
      if (missing_db_entry)
        CompleteRemoteSync(param.Pass(), SYNC_STATUS_OK);
      else
        DeleteMetadataForRemoteSync(param.Pass());
      return;
    case REMOTE_SYNC_OPERATION_FAIL:
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
  if (param->remote_change.sync_type !=
      RemoteChangeHandler::REMOTE_SYNC_TYPE_FETCH)
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
    DCHECK(IsSyncDirectoryOperationEnabled());
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
  if (param->remote_change.sync_type ==
      RemoteChangeHandler::REMOTE_SYNC_TYPE_INCREMENTAL) {
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
    int64 changestamp,
    RemoteChangeHandler::RemoteSyncType sync_type) {
  base::FilePath path = TitleToPath(entry.title());

  if (!entry.is_folder() && !entry.is_file() && !entry.deleted())
    return false;

  if (entry.is_folder() && !IsSyncDirectoryOperationEnabled())
    return false;

  SyncFileType file_type = entry.is_file() ?
      SYNC_FILE_TYPE_FILE : SYNC_FILE_TYPE_DIRECTORY;

  return AppendRemoteChangeInternal(
      origin, path, entry.deleted(),
      entry.resource_id(), changestamp,
      entry.deleted() ? std::string() : entry.file_md5(),
      entry.updated_time(), file_type, sync_type);
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
      type,
      RemoteChangeHandler::REMOTE_SYNC_TYPE_FETCH);
}

bool DriveFileSyncService::AppendRemoteChangeInternal(
    const GURL& origin,
    const base::FilePath& path,
    bool is_deleted,
    const std::string& remote_resource_id,
    int64 changestamp,
    const std::string& remote_file_md5,
    const base::Time& updated_time,
    SyncFileType file_type,
    RemoteChangeHandler::RemoteSyncType sync_type) {
  fileapi::FileSystemURL url(
      CreateSyncableFileSystemURL(origin, kServiceName, path));
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
      DCHECK(IsSyncDirectoryOperationEnabled() ||
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
      updated_time, sync_type, url, file_change);
  remote_change_handler_.AppendChange(remote_change);

  DVLOG(3) << "Append remote change: " << path.value()
           << " (" << normalized_path << ")"
           << "@" << changestamp << " "
           << file_change.DebugString();

  return true;
}

void DriveFileSyncService::RemoveRemoteChange(
    const FileSystemURL& url) {
  if (!remote_change_handler_.RemoveChangeForURL(url))
    return;
  if (metadata_store_->IsBatchSyncOrigin(url.origin()) &&
      !remote_change_handler_.HasChangesForOrigin(url.origin())) {
    metadata_store_->MoveBatchSyncOriginToIncremental(url.origin());
  }
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
  task_manager_->NotifyLastDriveError(error);
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK && !sync_client_->IsAuthenticated())
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
  // TODO(calvinlo): Try to eliminate may_have_unfetched_changes_ variable.
  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
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
    SyncStatusCode sync_status,
    google_apis::GDataErrorCode gdata_error) {
  UpdateServiceStateFromLastOperationStatus(sync_status, gdata_error);
}

void DriveFileSyncService::FetchChangesForIncrementalSync(
    const SyncStatusCallback& callback) {
  DCHECK(may_have_unfetched_changes_);
  DCHECK(pending_batch_sync_origins_.empty());
  DCHECK(!metadata_store_->incremental_sync_origins().empty());

  DVLOG(1) << "FetchChangesForIncrementalSync (start_changestamp:"
           << (largest_fetched_changestamp_ + 1) << ")";

  sync_client_->ListChanges(
      largest_fetched_changestamp_ + 1,
      base::Bind(&DriveFileSyncService::DidFetchChangesForIncrementalSync,
                 AsWeakPtr(), callback, false));

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
        origin, entry, entry.changestamp(),
        RemoteChangeHandler::REMOTE_SYNC_TYPE_INCREMENTAL) || has_new_changes;
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

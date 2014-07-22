// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_notification_manager.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/api_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/local_sync_delegate.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/remote_sync_delegate.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/browser/sync_file_system/sync_file_type.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/scoped_file.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::FileSystemURL;

namespace sync_file_system {

typedef RemoteFileSyncService::OriginStatusMap OriginStatusMap;

namespace {

const base::FilePath::CharType kTempDirName[] = FILE_PATH_LITERAL("tmp");

void EmptyStatusCallback(SyncStatusCode status) {}

}  // namespace

ConflictResolutionPolicy DriveFileSyncService::kDefaultPolicy =
    CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN;

// DriveFileSyncService ------------------------------------------------------

DriveFileSyncService::~DriveFileSyncService() {
  if (api_util_)
    api_util_->RemoveObserver(this);

  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::FindForBrowserContext(profile_);
  if (drive_notification_manager)
    drive_notification_manager->RemoveObserver(this);
}

scoped_ptr<DriveFileSyncService> DriveFileSyncService::Create(
    Profile* profile) {
  scoped_ptr<DriveFileSyncService> service(new DriveFileSyncService(profile));
  scoped_ptr<drive_backend::SyncTaskManager> task_manager(
      new drive_backend::SyncTaskManager(service->AsWeakPtr(),
                                         0 /* maximum_background_task */,
                                         base::ThreadTaskRunnerHandle::Get()));
  SyncStatusCallback callback = base::Bind(
      &drive_backend::SyncTaskManager::Initialize, task_manager->AsWeakPtr());
  service->Initialize(task_manager.Pass(), callback);
  return service.Pass();
}

void DriveFileSyncService::AppendDependsOnFactories(
    std::set<BrowserContextKeyedServiceFactory*>* factories) {
  DCHECK(factories);
  factories->insert(drive::DriveNotificationManagerFactory::GetInstance());
  factories->insert(ProfileOAuth2TokenServiceFactory::GetInstance());
  factories->insert(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

scoped_ptr<DriveFileSyncService> DriveFileSyncService::CreateForTesting(
    Profile* profile,
    const base::FilePath& base_dir,
    scoped_ptr<drive_backend::APIUtilInterface> api_util,
    scoped_ptr<DriveMetadataStore> metadata_store) {
  scoped_ptr<DriveFileSyncService> service(new DriveFileSyncService(profile));
  scoped_ptr<drive_backend::SyncTaskManager> task_manager(
      new drive_backend::SyncTaskManager(service->AsWeakPtr(),
                                         0 /* maximum_background_task */,
                                         base::ThreadTaskRunnerHandle::Get()));
  SyncStatusCallback callback = base::Bind(
      &drive_backend::SyncTaskManager::Initialize, task_manager->AsWeakPtr());
  service->InitializeForTesting(task_manager.Pass(),
                                base_dir,
                                api_util.Pass(),
                                metadata_store.Pass(),
                                callback);
  return service.Pass();
}

void DriveFileSyncService::AddServiceObserver(Observer* observer) {
  service_observers_.AddObserver(observer);
}

void DriveFileSyncService::AddFileStatusObserver(
    FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void DriveFileSyncService::RegisterOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  if (!pending_origin_operations_.HasPendingOperation(origin) &&
      metadata_store_->IsIncrementalSyncOrigin(origin) &&
      !metadata_store_->GetResourceIdForOrigin(origin).empty()) {
    DCHECK(!metadata_store_->IsOriginDisabled(origin));
    callback.Run(SYNC_STATUS_OK);
    MaybeStartFetchChanges();
    return;
  }

  pending_origin_operations_.Push(origin, OriginOperation::REGISTERING);

  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DoRegisterOrigin, AsWeakPtr(), origin),
      drive_backend::SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void DriveFileSyncService::EnableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  pending_origin_operations_.Push(origin, OriginOperation::ENABLING);
  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DoEnableOrigin, AsWeakPtr(), origin),
      drive_backend::SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void DriveFileSyncService::DisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  pending_origin_operations_.Push(origin, OriginOperation::DISABLING);
  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DoDisableOrigin, AsWeakPtr(), origin),
      drive_backend::SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void DriveFileSyncService::UninstallOrigin(
    const GURL& origin,
    UninstallFlag flag,
    const SyncStatusCallback& callback) {
  pending_origin_operations_.Push(origin, OriginOperation::UNINSTALLING);
  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DoUninstallOrigin, AsWeakPtr(),
                 origin, flag),
      drive_backend::SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void DriveFileSyncService::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DoProcessRemoteChange, AsWeakPtr(),
                 callback),
      drive_backend::SyncTaskManager::PRIORITY_MED,
      base::Bind(&EmptyStatusCallback));
}

void DriveFileSyncService::SetRemoteChangeProcessor(
    RemoteChangeProcessor* processor) {
  remote_change_processor_ = processor;
}

LocalChangeProcessor* DriveFileSyncService::GetLocalChangeProcessor() {
  return this;
}

RemoteServiceState DriveFileSyncService::GetCurrentState() const {
  if (!sync_enabled_)
    return REMOTE_SERVICE_DISABLED;
  return state_;
}

void DriveFileSyncService::GetOriginStatusMap(
    const StatusMapCallback& callback) {
  scoped_ptr<OriginStatusMap> status_map(new OriginStatusMap);

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

  callback.Run(status_map.Pass());
}

void DriveFileSyncService::DumpFiles(const GURL& origin,
                                     const ListCallback& callback) {
  callback.Run(metadata_store_->DumpFiles(origin).Pass());
}

void DriveFileSyncService::DumpDatabase(const ListCallback& callback) {
  // Not implemented (yet).
  callback.Run(scoped_ptr<base::ListValue>());
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

void DriveFileSyncService::PromoteDemotedChanges(
    const base::Closure& callback) {
  callback.Run();
}

void DriveFileSyncService::ApplyLocalChange(
    const FileChange& local_file_change,
    const base::FilePath& local_file_path,
    const SyncFileMetadata& local_file_metadata,
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&DriveFileSyncService::DoApplyLocalChange, AsWeakPtr(),
                 local_file_change,
                 local_file_path,
                 local_file_metadata,
                 url),
      drive_backend::SyncTaskManager::PRIORITY_MED,
      callback);
}

void DriveFileSyncService::OnAuthenticated() {
  if (state_ == REMOTE_SERVICE_OK)
    return;
  util::Log(logging::LOG_VERBOSE, FROM_HERE, "OnAuthenticated");

  UpdateServiceState(REMOTE_SERVICE_OK, "Authenticated");

  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

void DriveFileSyncService::OnNetworkConnected() {
  if (state_ == REMOTE_SERVICE_OK)
    return;
  util::Log(logging::LOG_VERBOSE, FROM_HERE, "OnNetworkConnected");

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
      conflict_resolution_resolver_(kDefaultPolicy) {
}

void DriveFileSyncService::Initialize(
    scoped_ptr<drive_backend::SyncTaskManager> task_manager,
    const SyncStatusCallback& callback) {
  DCHECK(profile_);
  DCHECK(!metadata_store_);
  DCHECK(!task_manager_);

  task_manager_ = task_manager.Pass();

  temporary_file_dir_ = sync_file_system::GetSyncFileSystemDir(
      profile_->GetPath()).Append(kTempDirName);

  api_util_.reset(new drive_backend::APIUtil(profile_, temporary_file_dir_));
  api_util_->AddObserver(this);

  metadata_store_.reset(new DriveMetadataStore(
      GetSyncFileSystemDir(profile_->GetPath()),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE).get()));

  metadata_store_->Initialize(
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore,
                 AsWeakPtr(), callback));
}

void DriveFileSyncService::InitializeForTesting(
    scoped_ptr<drive_backend::SyncTaskManager> task_manager,
    const base::FilePath& base_dir,
    scoped_ptr<drive_backend::APIUtilInterface> api_util,
    scoped_ptr<DriveMetadataStore> metadata_store,
    const SyncStatusCallback& callback) {
  DCHECK(!metadata_store_);
  DCHECK(!task_manager_);

  task_manager_ = task_manager.Pass();
  temporary_file_dir_ = base_dir.Append(kTempDirName);

  api_util_ = api_util.Pass();
  metadata_store_ = metadata_store.Pass();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::GetForBrowserContext(profile_);
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
      if (drive_backend::GDataErrorCodeToSyncStatusCode(gdata_error) ==
          SYNC_STATUS_OK)
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
    case SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE:
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
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "Service state changed: %d->%d: %s",
              old_state, GetCurrentState(), description.c_str());
    FOR_EACH_OBSERVER(
        Observer, service_observers_,
        OnRemoteServiceStateUpdated(GetCurrentState(), description));
  }
}

void DriveFileSyncService::DoRegisterOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));

  OriginOperation op = pending_origin_operations_.Pop();
  DCHECK_EQ(origin, op.origin);
  DCHECK_EQ(OriginOperation::REGISTERING, op.type);

  DCHECK(!metadata_store_->IsOriginDisabled(origin));
  if (!metadata_store_->GetResourceIdForOrigin(origin).empty()) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  EnsureOriginRootDirectory(
      origin, base::Bind(&DriveFileSyncService::DidGetDriveDirectoryForOrigin,
                         AsWeakPtr(), origin, callback));
}

void DriveFileSyncService::DoEnableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  OriginOperation op = pending_origin_operations_.Pop();
  DCHECK_EQ(origin, op.origin);
  DCHECK_EQ(OriginOperation::ENABLING, op.type);

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

void DriveFileSyncService::DoDisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  OriginOperation op = pending_origin_operations_.Pop();
  DCHECK_EQ(origin, op.origin);
  DCHECK_EQ(OriginOperation::DISABLING, op.type);

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
    UninstallFlag flag,
    const SyncStatusCallback& callback) {
  OriginOperation op = pending_origin_operations_.Pop();
  DCHECK_EQ(origin, op.origin);
  DCHECK_EQ(OriginOperation::UNINSTALLING, op.type);

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
    if (metadata_store_->IsKnownOrigin(origin))
      DidUninstallOrigin(origin, callback, google_apis::HTTP_SUCCESS);
    else
      callback.Run(SYNC_STATUS_UNKNOWN_ORIGIN);
    return;
  }

  if (flag == UNINSTALL_AND_KEEP_REMOTE) {
    DidUninstallOrigin(origin, callback, google_apis::HTTP_SUCCESS);
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

  SyncStatusCallback callback = base::Bind(
      &DriveFileSyncService::DidProcessRemoteChange, AsWeakPtr(),
      sync_callback, completion_callback);

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED) {
    callback.Run(SYNC_STATUS_SYNC_DISABLED);
    return;
  }

  if (!remote_change_handler_.HasChanges()) {
    callback.Run(SYNC_STATUS_NO_CHANGE_TO_SYNC);
    return;
  }

  RemoteChangeHandler::RemoteChange remote_change;
  bool has_remote_change =
      remote_change_handler_.GetChange(&remote_change);
  DCHECK(has_remote_change);

  DCHECK(!running_remote_sync_task_);
  running_remote_sync_task_.reset(new drive_backend::RemoteSyncDelegate(
      this, remote_change));
  running_remote_sync_task_->Run(callback);
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
  running_local_sync_task_.reset(new drive_backend::LocalSyncDelegate(
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
      // (At this stage we can't know if it was unpacked extension or not,
      // so just purge the remote folder.)
      UninstallOrigin(origin,
                      RemoteFileSyncService::UNINSTALL_AND_PURGE_REMOTE,
                      base::Bind(&EmptyStatusCallback));
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
  remote_change_handler_.RemoveChangesForOrigin(origin);
  pending_batch_sync_origins_.erase(origin);
  metadata_store_->RemoveOrigin(origin, callback);
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
    else
      continue;

    DCHECK(file_type == SYNC_FILE_TYPE_FILE ||
           file_type == SYNC_FILE_TYPE_DIRECTORY);

    // Save to be fetched file to DB for restore in case of crash.
    DriveMetadata metadata;
    metadata.set_resource_id(entry.resource_id());
    metadata.set_md5_checksum(std::string());
    metadata.set_conflicted(false);
    metadata.set_to_be_fetched(true);

    if (file_type == SYNC_FILE_TYPE_FILE)
      metadata.set_type(DriveMetadata::RESOURCE_TYPE_FILE);
    else
      metadata.set_type(DriveMetadata::RESOURCE_TYPE_FOLDER);

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

void DriveFileSyncService::DidProcessRemoteChange(
    const SyncFileCallback& sync_callback,
    const SyncStatusCallback& completion_callback,
    SyncStatusCode status) {
  fileapi::FileSystemURL url;
  if (running_remote_sync_task_)
    url = running_remote_sync_task_->url();
  running_remote_sync_task_.reset();

  completion_callback.Run(status);
  sync_callback.Run(status, url);
}

void DriveFileSyncService::DidApplyLocalChange(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  running_local_sync_task_.reset();
  callback.Run(status);
}

bool DriveFileSyncService::AppendRemoteChange(
    const GURL& origin,
    const google_apis::ResourceEntry& entry,
    int64 changestamp) {
  base::FilePath path = TitleToPath(entry.title());

  if (!entry.is_folder() && !entry.is_file() && !entry.deleted())
    return false;

  if (entry.is_folder())
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

  // Drop the change if remote change is for directory addition that is
  // already known.
  if (file_type == SYNC_FILE_TYPE_DIRECTORY &&
      !is_deleted &&
      !local_resource_id.empty() &&
      metadata.type() == DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER)
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
      DCHECK(DriveMetadata::RESOURCE_TYPE_FILE == metadata.type());
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
  DCHECK(!drive_metadata->resource_id().empty());
  drive_metadata->set_conflicted(true);
  drive_metadata->set_to_be_fetched(false);
  metadata_store_->UpdateEntry(
      url, *drive_metadata, base::Bind(
          &DriveFileSyncService::NotifyConflict,
          AsWeakPtr(), url, callback));
}

void DriveFileSyncService::NotifyConflict(
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }
  NotifyObserversFileStatusChanged(url,
                                   SYNC_FILE_STATUS_CONFLICTING,
                                   SYNC_ACTION_NONE,
                                   SYNC_DIRECTION_NONE);
  callback.Run(status);
}

SyncStatusCode DriveFileSyncService::GDataErrorCodeToSyncStatusCodeWrapper(
    google_apis::GDataErrorCode error) {
  last_gdata_error_ = error;
  SyncStatusCode status = drive_backend::GDataErrorCodeToSyncStatusCode(error);
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
          FROM_HERE,
          base::Bind(&DriveFileSyncService::StartBatchSync, AsWeakPtr()),
          SyncStatusCallback());
    }
    return;
  }

  if (may_have_unfetched_changes_ &&
      !metadata_store_->incremental_sync_origins().empty()) {
    task_manager_->ScheduleTaskIfIdle(
        FROM_HERE,
        base::Bind(&DriveFileSyncService::FetchChangesForIncrementalSync,
                   AsWeakPtr()),
        SyncStatusCallback());
  }
}

void DriveFileSyncService::OnNotificationReceived() {
  VLOG(2) << "Notification received to check for Google Drive updates";

  // Likely indicating the network is enabled again.
  UpdateServiceState(REMOTE_SERVICE_OK, "Got push notification for Drive.");

  // TODO(calvinlo): Try to eliminate may_have_unfetched_changes_ variable.
  may_have_unfetched_changes_ = true;
  MaybeStartFetchChanges();
}

void DriveFileSyncService::OnPushNotificationEnabled(bool enabled) {
  VLOG(2) << "XMPP Push notification is " << (enabled ? "enabled" : "disabled");
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
    bool used_network) {
  UpdateServiceStateFromLastOperationStatus(sync_status, last_gdata_error_);
}

void DriveFileSyncService::RecordTaskLog(scoped_ptr<TaskLogger::TaskLog> log) {
  // V1 backend doesn't support per-task logging.
}

// static
std::string DriveFileSyncService::PathToTitle(const base::FilePath& path) {
  return path.AsUTF8Unsafe();
}

// static
base::FilePath DriveFileSyncService::TitleToPath(const std::string& title) {
  return base::FilePath::FromUTF8Unsafe(title);
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
        drive::util::ExtractResourceIdFromUrl((*itr)->href()));
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

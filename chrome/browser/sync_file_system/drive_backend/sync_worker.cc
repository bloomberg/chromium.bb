// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_worker.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/drive/drive_notification_manager.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/uninstall_app_task.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"
#include "webkit/common/blob/scoped_file.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

namespace {

void EmptyStatusCallback(SyncStatusCode status) {}

}  // namespace

scoped_ptr<SyncWorker> SyncWorker::CreateOnWorker(
    const base::WeakPtr<drive_backend::SyncEngine>& sync_engine,
    const base::FilePath& base_dir,
    scoped_ptr<drive::DriveServiceInterface> drive_service,
    scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
    base::SequencedTaskRunner* task_runner,
    leveldb::Env* env_override) {
  scoped_ptr<SyncWorker> sync_worker(
      new SyncWorker(sync_engine,
                     base_dir,
                     drive_service.Pass(),
                     drive_uploader.Pass(),
                     task_runner,
                     env_override));
  sync_worker->Initialize();

  return sync_worker.Pass();
}

SyncWorker::~SyncWorker() {}

void SyncWorker::Initialize() {
  DCHECK(!task_manager_);

  task_manager_.reset(new SyncTaskManager(
      weak_ptr_factory_.GetWeakPtr(), 0 /* maximum_background_task */));
  task_manager_->Initialize(SYNC_STATUS_OK);

  PostInitializeTask();

  net::NetworkChangeNotifier::ConnectionType type =
      net::NetworkChangeNotifier::GetConnectionType();
  network_available_ =
      type != net::NetworkChangeNotifier::CONNECTION_NONE;
}

void SyncWorker::RegisterOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  if (!GetMetadataDatabase() && GetDriveService()->HasRefreshToken())
    PostInitializeTask();

  scoped_ptr<RegisterAppTask> task(
      new RegisterAppTask(context_.get(), origin.host()));
  if (task->CanFinishImmediately()) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      task.PassAs<SyncTask>(),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncWorker::EnableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&SyncWorker::DoEnableApp,
                 weak_ptr_factory_.GetWeakPtr(),
                 origin.host()),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncWorker::DisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTask(
      FROM_HERE,
      base::Bind(&SyncWorker::DoDisableApp,
                 weak_ptr_factory_.GetWeakPtr(),
                 origin.host()),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncWorker::UninstallOrigin(
    const GURL& origin,
    RemoteFileSyncService::UninstallFlag flag,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(
          new UninstallAppTask(context_.get(), origin.host(), flag)),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncWorker::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  RemoteToLocalSyncer* syncer = new RemoteToLocalSyncer(context_.get());
  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(syncer),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncWorker::DidProcessRemoteChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer, callback));
}

void SyncWorker::SetRemoteChangeProcessor(
    RemoteChangeProcessor* processor) {
  context_->SetRemoteChangeProcessor(processor);
}

RemoteServiceState SyncWorker::GetCurrentState() const {
  if (!sync_enabled_)
    return REMOTE_SERVICE_DISABLED;
  return service_state_;
}

void SyncWorker::GetOriginStatusMap(
    RemoteFileSyncService::OriginStatusMap* status_map) {
  DCHECK(status_map);

  if (!GetMetadataDatabase())
    return;

  std::vector<std::string> app_ids;
  GetMetadataDatabase()->GetRegisteredAppIDs(&app_ids);

  for (std::vector<std::string>::const_iterator itr = app_ids.begin();
       itr != app_ids.end(); ++itr) {
    const std::string& app_id = *itr;
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(app_id);
    (*status_map)[origin] =
        GetMetadataDatabase()->IsAppEnabled(app_id) ?
        "Enabled" : "Disabled";
  }
}

scoped_ptr<base::ListValue> SyncWorker::DumpFiles(const GURL& origin) {
  if (!GetMetadataDatabase())
    return scoped_ptr<base::ListValue>();
  return GetMetadataDatabase()->DumpFiles(origin.host());
}

scoped_ptr<base::ListValue> SyncWorker::DumpDatabase() {
  if (!GetMetadataDatabase())
    return scoped_ptr<base::ListValue>();
  return GetMetadataDatabase()->DumpDatabase();
}

void SyncWorker::SetSyncEnabled(bool enabled) {
  if (sync_enabled_ == enabled)
    return;

  RemoteServiceState old_state = GetCurrentState();
  sync_enabled_ = enabled;
  if (old_state == GetCurrentState())
    return;

  // TODO(peria): PostTask()
  sync_engine_->UpdateSyncEnabled(enabled);
}

SyncStatusCode SyncWorker::SetDefaultConflictResolutionPolicy(
    ConflictResolutionPolicy policy) {
  default_conflict_resolution_policy_ = policy;
  return SYNC_STATUS_OK;
}

SyncStatusCode SyncWorker::SetConflictResolutionPolicy(
    const GURL& origin,
    ConflictResolutionPolicy policy) {
  NOTIMPLEMENTED();
  default_conflict_resolution_policy_ = policy;
  return SYNC_STATUS_OK;
}

ConflictResolutionPolicy SyncWorker::GetDefaultConflictResolutionPolicy()
    const {
  return default_conflict_resolution_policy_;
}

ConflictResolutionPolicy SyncWorker::GetConflictResolutionPolicy(
    const GURL& origin) const {
  NOTIMPLEMENTED();
  return default_conflict_resolution_policy_;
}

void SyncWorker::ApplyLocalChange(
    const FileChange& local_change,
    const base::FilePath& local_path,
    const SyncFileMetadata& local_metadata,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  LocalToRemoteSyncer* syncer = new LocalToRemoteSyncer(
      context_.get(), local_metadata, local_change, local_path, url);
  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(syncer),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncWorker::DidApplyLocalChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer, callback));
}

void SyncWorker::MaybeScheduleNextTask() {
  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  // TODO(tzik): Notify observer of OnRemoteChangeQueueUpdated.
  // TODO(tzik): Add an interface to get the number of dirty trackers to
  // MetadataDatabase.

  MaybeStartFetchChanges();
}

void SyncWorker::NotifyLastOperationStatus(
    SyncStatusCode status,
    bool used_network) {
  UpdateServiceStateFromSyncStatusCode(status, used_network);

  if (GetMetadataDatabase()) {
    // TODO(peria): Post task
    sync_engine_->NotifyLastOperationStatus();
  }
}

void SyncWorker::OnNotificationReceived() {
  if (service_state_ == REMOTE_SERVICE_TEMPORARY_UNAVAILABLE)
    UpdateServiceState(REMOTE_SERVICE_OK, "Got push notification for Drive.");

  should_check_remote_change_ = true;
  MaybeScheduleNextTask();
}

void SyncWorker::OnReadyToSendRequests(const std::string& account_id) {
  if (service_state_ == REMOTE_SERVICE_OK)
    return;
  UpdateServiceState(REMOTE_SERVICE_OK, "Authenticated");

  if (!GetMetadataDatabase() && !account_id.empty()) {
    GetDriveService()->Initialize(account_id);
    PostInitializeTask();
    return;
  }

  should_check_remote_change_ = true;
  MaybeScheduleNextTask();
}

void SyncWorker::OnRefreshTokenInvalid() {
  UpdateServiceState(
      REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
      "Found invalid refresh token.");
}

void SyncWorker::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  bool new_network_availability =
      type != net::NetworkChangeNotifier::CONNECTION_NONE;

  if (network_available_ && !new_network_availability) {
    UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE, "Disconnected");
  } else if (!network_available_ && new_network_availability) {
    UpdateServiceState(REMOTE_SERVICE_OK, "Connected");
    should_check_remote_change_ = true;
    MaybeStartFetchChanges();
  }
  network_available_ = new_network_availability;
}

drive::DriveServiceInterface* SyncWorker::GetDriveService() {
  return context_->GetDriveService();
}

drive::DriveUploaderInterface* SyncWorker::GetDriveUploader() {
  return context_->GetDriveUploader();
}

MetadataDatabase* SyncWorker::GetMetadataDatabase() {
  return context_->GetMetadataDatabase();
}

SyncTaskManager* SyncWorker::GetSyncTaskManager() {
  return task_manager_.get();
}

SyncWorker::SyncWorker(
    const base::WeakPtr<drive_backend::SyncEngine>& sync_engine,
    const base::FilePath& base_dir,
    scoped_ptr<drive::DriveServiceInterface> drive_service,
    scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
    base::SequencedTaskRunner* task_runner,
    leveldb::Env* env_override)
    : base_dir_(base_dir),
      env_override_(env_override),
      service_state_(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE),
      should_check_conflict_(true),
      should_check_remote_change_(true),
      listing_remote_changes_(false),
      sync_enabled_(false),
      default_conflict_resolution_policy_(
          CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN),
      network_available_(false),
      context_(new SyncEngineContext(drive_service.Pass(),
                                     drive_uploader.Pass(),
                                     task_runner)),
      sync_engine_(sync_engine),
      weak_ptr_factory_(this) {}

void SyncWorker::DoDisableApp(const std::string& app_id,
                              const SyncStatusCallback& callback) {
  if (GetMetadataDatabase())
    GetMetadataDatabase()->DisableApp(app_id, callback);
  else
    callback.Run(SYNC_STATUS_OK);
}

void SyncWorker::DoEnableApp(const std::string& app_id,
                             const SyncStatusCallback& callback) {
  if (GetMetadataDatabase())
    GetMetadataDatabase()->EnableApp(app_id, callback);
  else
    callback.Run(SYNC_STATUS_OK);
}

void SyncWorker::PostInitializeTask() {
  DCHECK(!GetMetadataDatabase());

  // This initializer task may not run if MetadataDatabase in context_ is
  // already initialized when it runs.
  SyncEngineInitializer* initializer =
      new SyncEngineInitializer(context_.get(),
                                context_->GetBlockingTaskRunner(),
                                base_dir_.Append(kDatabaseName),
                                env_override_);
  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(initializer),
      SyncTaskManager::PRIORITY_HIGH,
      base::Bind(&SyncWorker::DidInitialize, weak_ptr_factory_.GetWeakPtr(),
                 initializer));
}

void SyncWorker::DidInitialize(SyncEngineInitializer* initializer,
                               SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    if (GetDriveService()->HasRefreshToken()) {
      UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
                         "Could not initialize remote service");
    } else {
      UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                         "Authentication required.");
    }
    return;
  }

  scoped_ptr<MetadataDatabase> metadata_database =
      initializer->PassMetadataDatabase();
  if (metadata_database)
    context_->SetMetadataDatabase(metadata_database.Pass());

  // TODO(peria): Post task
  sync_engine_->UpdateRegisteredApps();
}

void SyncWorker::DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                                        const SyncFileCallback& callback,
                                        SyncStatusCode status) {
  if (syncer->is_sync_root_deletion()) {
    MetadataDatabase::ClearDatabase(context_->PassMetadataDatabase());
    PostInitializeTask();
    callback.Run(status, syncer->url());
    return;
  }

  if (status == SYNC_STATUS_OK) {
    // TODO(peria): Post task
    sync_engine_->DidProcessRemoteChange(syncer);

    if (syncer->sync_action() == SYNC_ACTION_DELETED &&
        syncer->url().is_valid() &&
        fileapi::VirtualPath::IsRootPath(syncer->url().path())) {
      RegisterOrigin(syncer->url().origin(), base::Bind(&EmptyStatusCallback));
    }
    should_check_conflict_ = true;
  }
  callback.Run(status, syncer->url());
}

void SyncWorker::DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                                     const SyncStatusCallback& callback,
                                     SyncStatusCode status) {
  // TODO(peria): Post task
  sync_engine_->DidApplyLocalChange(syncer, status);

  if (status == SYNC_STATUS_UNKNOWN_ORIGIN && syncer->url().is_valid()) {
    RegisterOrigin(syncer->url().origin(),
                   base::Bind(&EmptyStatusCallback));
  }

  if (syncer->needs_remote_change_listing() &&
      !listing_remote_changes_) {
    task_manager_->ScheduleSyncTask(
        FROM_HERE,
        scoped_ptr<SyncTask>(new ListChangesTask(context_.get())),
        SyncTaskManager::PRIORITY_HIGH,
        base::Bind(&SyncWorker::DidFetchChanges,
                   weak_ptr_factory_.GetWeakPtr()));
    should_check_remote_change_ = false;
    listing_remote_changes_ = true;
    time_to_check_changes_ =
        base::TimeTicks::Now() +
        base::TimeDelta::FromSeconds(kListChangesRetryDelaySeconds);
  }

  if (status != SYNC_STATUS_OK &&
      status != SYNC_STATUS_NO_CHANGE_TO_SYNC) {
    callback.Run(status);
    return;
  }

  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;

  callback.Run(status);
}

void SyncWorker::MaybeStartFetchChanges() {
  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  if (!GetMetadataDatabase())
    return;

  if (listing_remote_changes_)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!should_check_remote_change_ && now < time_to_check_changes_) {
    if (!GetMetadataDatabase()->HasDirtyTracker() &&
        should_check_conflict_) {
      should_check_conflict_ = false;
      task_manager_->ScheduleSyncTaskIfIdle(
          FROM_HERE,
          scoped_ptr<SyncTask>(new ConflictResolver(context_.get())),
          base::Bind(&SyncWorker::DidResolveConflict,
                     weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  if (task_manager_->ScheduleSyncTaskIfIdle(
          FROM_HERE,
          scoped_ptr<SyncTask>(new ListChangesTask(context_.get())),
          base::Bind(&SyncWorker::DidFetchChanges,
                     weak_ptr_factory_.GetWeakPtr()))) {
    should_check_remote_change_ = false;
    listing_remote_changes_ = true;
    time_to_check_changes_ =
        now + base::TimeDelta::FromSeconds(kListChangesRetryDelaySeconds);
  }
}

void SyncWorker::DidResolveConflict(SyncStatusCode status) {
  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;
}

void SyncWorker::DidFetchChanges(SyncStatusCode status) {
  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;
  listing_remote_changes_ = false;
}

void SyncWorker::UpdateServiceStateFromSyncStatusCode(
    SyncStatusCode status,
    bool used_network) {
  switch (status) {
    case SYNC_STATUS_OK:
      if (used_network)
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
      if (GetDriveService()->HasRefreshToken()) {
        UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
                           "Network or temporary service error.");
      } else {
        UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                           "Authentication required");
      }
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

void SyncWorker::UpdateServiceState(RemoteServiceState state,
                                    const std::string& description) {
  RemoteServiceState old_state = GetCurrentState();
  service_state_ = state;

  if (old_state == GetCurrentState())
    return;

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Service state changed: %d->%d: %s",
            old_state, GetCurrentState(), description.c_str());
  // TODO(peria): Post task
  sync_engine_->UpdateServiceState(description);
}

}  // namespace drive_backend
}  // namespace sync_file_system

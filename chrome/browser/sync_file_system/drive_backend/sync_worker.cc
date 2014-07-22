// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_worker.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_on_worker.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/uninstall_app_task.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

namespace {

void EmptyStatusCallback(SyncStatusCode status) {}

}  // namespace

SyncWorker::SyncWorker(
    const base::FilePath& base_dir,
    const base::WeakPtr<ExtensionServiceInterface>& extension_service,
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
      extension_service_(extension_service),
      has_refresh_token_(false),
      weak_ptr_factory_(this) {
  sequence_checker_.DetachFromSequence();
  DCHECK(base_dir_.IsAbsolute());
}

SyncWorker::~SyncWorker() {
  observers_.Clear();
}

void SyncWorker::Initialize(scoped_ptr<SyncEngineContext> context) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(!task_manager_);

  context_ = context.Pass();

  task_manager_.reset(new SyncTaskManager(
      weak_ptr_factory_.GetWeakPtr(), 0 /* maximum_background_task */,
      context_->GetWorkerTaskRunner()));
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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (!GetMetadataDatabase() && has_refresh_token_)
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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(
          new UninstallAppTask(context_.get(), origin.host(), flag)),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncWorker::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  RemoteToLocalSyncer* syncer = new RemoteToLocalSyncer(context_.get());
  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(syncer),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncWorker::DidProcessRemoteChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer,
                 callback));
}

void SyncWorker::SetRemoteChangeProcessor(
    RemoteChangeProcessorOnWorker* remote_change_processor_on_worker) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  context_->SetRemoteChangeProcessor(remote_change_processor_on_worker);
}

RemoteServiceState SyncWorker::GetCurrentState() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (!sync_enabled_)
    return REMOTE_SERVICE_DISABLED;
  return service_state_;
}

void SyncWorker::GetOriginStatusMap(
    const RemoteFileSyncService::StatusMapCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (!GetMetadataDatabase())
    return;

  std::vector<std::string> app_ids;
  GetMetadataDatabase()->GetRegisteredAppIDs(&app_ids);

  scoped_ptr<RemoteFileSyncService::OriginStatusMap>
      status_map(new RemoteFileSyncService::OriginStatusMap);
  for (std::vector<std::string>::const_iterator itr = app_ids.begin();
       itr != app_ids.end(); ++itr) {
    const std::string& app_id = *itr;
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);
    (*status_map)[origin] =
        GetMetadataDatabase()->IsAppEnabled(app_id) ? "Enabled" : "Disabled";
  }

  callback.Run(status_map.Pass());
}

scoped_ptr<base::ListValue> SyncWorker::DumpFiles(const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (!GetMetadataDatabase())
    return scoped_ptr<base::ListValue>();
  return GetMetadataDatabase()->DumpFiles(origin.host());
}

scoped_ptr<base::ListValue> SyncWorker::DumpDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (!GetMetadataDatabase())
    return scoped_ptr<base::ListValue>();
  return GetMetadataDatabase()->DumpDatabase();
}

void SyncWorker::SetSyncEnabled(bool enabled) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (sync_enabled_ == enabled)
    return;

  RemoteServiceState old_state = GetCurrentState();
  sync_enabled_ = enabled;
  if (old_state == GetCurrentState())
    return;

  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      UpdateServiceState(
          GetCurrentState(),
          enabled ? "Sync is enabled" : "Sync is disabled"));
}

void SyncWorker::PromoteDemotedChanges(const base::Closure& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  MetadataDatabase* metadata_db = GetMetadataDatabase();
  if (metadata_db && metadata_db->HasLowPriorityDirtyTracker()) {
    metadata_db->PromoteLowerPriorityTrackersToNormal();
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnPendingFileListUpdated(metadata_db->CountDirtyTracker()));
  }
  callback.Run();
}

void SyncWorker::ApplyLocalChange(
    const FileChange& local_change,
    const base::FilePath& local_path,
    const SyncFileMetadata& local_metadata,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  LocalToRemoteSyncer* syncer = new LocalToRemoteSyncer(
      context_.get(), local_metadata, local_change, local_path, url);
  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(syncer),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncWorker::DidApplyLocalChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer,
                 callback));
}

void SyncWorker::MaybeScheduleNextTask() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  UpdateServiceStateFromSyncStatusCode(status, used_network);

  if (GetMetadataDatabase()) {
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OnPendingFileListUpdated(GetMetadataDatabase()->CountDirtyTracker()));
  }
}

void SyncWorker::RecordTaskLog(scoped_ptr<TaskLogger::TaskLog> task_log) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  context_->GetUITaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&TaskLogger::RecordLog,
                 context_->GetTaskLogger(),
                 base::Passed(&task_log)));
}

void SyncWorker::OnNotificationReceived() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (service_state_ == REMOTE_SERVICE_TEMPORARY_UNAVAILABLE)
    UpdateServiceState(REMOTE_SERVICE_OK, "Got push notification for Drive.");

  should_check_remote_change_ = true;
  MaybeScheduleNextTask();
}

void SyncWorker::OnReadyToSendRequests() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  has_refresh_token_ = true;

  if (service_state_ == REMOTE_SERVICE_OK)
    return;
  UpdateServiceState(REMOTE_SERVICE_OK, "Authenticated");

  if (!GetMetadataDatabase()) {
    PostInitializeTask();
    return;
  }

  should_check_remote_change_ = true;
  MaybeScheduleNextTask();
}

void SyncWorker::OnRefreshTokenInvalid() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  has_refresh_token_ = false;

  UpdateServiceState(
      REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
      "Found invalid refresh token.");
}

void SyncWorker::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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

void SyncWorker::DetachFromSequence() {
  task_manager_->DetachFromSequence();
  context_->DetachFromSequence();
  sequence_checker_.DetachFromSequence();
}

void SyncWorker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncWorker::SetHasRefreshToken(bool has_refresh_token) {
  has_refresh_token_ = has_refresh_token;
}

void SyncWorker::DoDisableApp(const std::string& app_id,
                              const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (GetMetadataDatabase()) {
    GetMetadataDatabase()->DisableApp(app_id, callback);
  } else {
    callback.Run(SYNC_STATUS_OK);
  }
}

void SyncWorker::DoEnableApp(const std::string& app_id,
                             const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (GetMetadataDatabase()) {
    GetMetadataDatabase()->EnableApp(app_id, callback);
  } else {
    callback.Run(SYNC_STATUS_OK);
  }
}

void SyncWorker::PostInitializeTask() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(!GetMetadataDatabase());

  // This initializer task may not run if MetadataDatabase in context_ is
  // already initialized when it runs.
  SyncEngineInitializer* initializer =
      new SyncEngineInitializer(context_.get(),
                                base_dir_.Append(kDatabaseName),
                                env_override_);
  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(initializer),
      SyncTaskManager::PRIORITY_HIGH,
      base::Bind(&SyncWorker::DidInitialize,
                 weak_ptr_factory_.GetWeakPtr(),
                 initializer));
}

void SyncWorker::DidInitialize(SyncEngineInitializer* initializer,
                               SyncStatusCode status) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (status != SYNC_STATUS_OK) {
    if (has_refresh_token_) {
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

  UpdateRegisteredApps();
}

void SyncWorker::UpdateRegisteredApps() {
  MetadataDatabase* metadata_db = GetMetadataDatabase();
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(metadata_db);

  scoped_ptr<std::vector<std::string> > app_ids(new std::vector<std::string>);
  metadata_db->GetRegisteredAppIDs(app_ids.get());

  AppStatusMap* app_status = new AppStatusMap;
  base::Closure callback =
      base::Bind(&SyncWorker::DidQueryAppStatus,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(app_status));

  context_->GetUITaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::QueryAppStatusOnUIThread,
                 extension_service_,
                 base::Owned(app_ids.release()),
                 app_status,
                 RelayCallbackToTaskRunner(
                     context_->GetWorkerTaskRunner(),
                     FROM_HERE, callback)));
}

void SyncWorker::QueryAppStatusOnUIThread(
    const base::WeakPtr<ExtensionServiceInterface>& extension_service_ptr,
    const std::vector<std::string>* app_ids,
    AppStatusMap* status,
    const base::Closure& callback) {
  ExtensionServiceInterface* extension_service = extension_service_ptr.get();
  if (!extension_service) {
    callback.Run();
    return;
  }

  for (std::vector<std::string>::const_iterator itr = app_ids->begin();
       itr != app_ids->end(); ++itr) {
    const std::string& app_id = *itr;
    if (!extension_service->GetInstalledExtension(app_id))
      (*status)[app_id] = APP_STATUS_UNINSTALLED;
    else if (!extension_service->IsExtensionEnabled(app_id))
      (*status)[app_id] = APP_STATUS_DISABLED;
    else
      (*status)[app_id] = APP_STATUS_ENABLED;
  }

  callback.Run();
}

void SyncWorker::DidQueryAppStatus(const AppStatusMap* app_status) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  MetadataDatabase* metadata_db = GetMetadataDatabase();
  DCHECK(metadata_db);

  // Update the status of every origin using status from ExtensionService.
  for (AppStatusMap::const_iterator itr = app_status->begin();
       itr != app_status->end(); ++itr) {
    const std::string& app_id = itr->first;
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);

    if (itr->second == APP_STATUS_UNINSTALLED) {
      // Extension has been uninstalled.
      // (At this stage we can't know if it was unpacked extension or not,
      // so just purge the remote folder.)
      UninstallOrigin(origin,
                      RemoteFileSyncService::UNINSTALL_AND_PURGE_REMOTE,
                      base::Bind(&EmptyStatusCallback));
      continue;
    }

    FileTracker tracker;
    if (!metadata_db->FindAppRootTracker(app_id, &tracker)) {
      // App will register itself on first run.
      continue;
    }

    DCHECK(itr->second == APP_STATUS_ENABLED ||
           itr->second == APP_STATUS_DISABLED);
    bool is_app_enabled = (itr->second == APP_STATUS_ENABLED);
    bool is_app_root_tracker_enabled =
        (tracker.tracker_kind() == TRACKER_KIND_APP_ROOT);
    if (is_app_enabled && !is_app_root_tracker_enabled)
      EnableOrigin(origin, base::Bind(&EmptyStatusCallback));
    else if (!is_app_enabled && is_app_root_tracker_enabled)
      DisableOrigin(origin, base::Bind(&EmptyStatusCallback));
  }
}

void SyncWorker::DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                                        const SyncFileCallback& callback,
                                        SyncStatusCode status) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (syncer->is_sync_root_deletion()) {
    MetadataDatabase::ClearDatabase(context_->PassMetadataDatabase());
    PostInitializeTask();
    callback.Run(status, syncer->url());
    return;
  }

  if (status == SYNC_STATUS_OK) {
    if (syncer->sync_action() != SYNC_ACTION_NONE &&
        syncer->url().is_valid()) {
      FOR_EACH_OBSERVER(
          Observer, observers_,
          OnFileStatusChanged(
              syncer->url(),
              SYNC_FILE_STATUS_SYNCED,
              syncer->sync_action(),
              SYNC_DIRECTION_REMOTE_TO_LOCAL));
    }

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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if ((status == SYNC_STATUS_OK || status == SYNC_STATUS_RETRY) &&
      syncer->url().is_valid() &&
      syncer->sync_action() != SYNC_ACTION_NONE) {
    fileapi::FileSystemURL updated_url = syncer->url();
    if (!syncer->target_path().empty()) {
      updated_url = CreateSyncableFileSystemURL(syncer->url().origin(),
                                                syncer->target_path());
    }
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnFileStatusChanged(updated_url,
                                          SYNC_FILE_STATUS_SYNCED,
                                          syncer->sync_action(),
                                          SYNC_DIRECTION_LOCAL_TO_REMOTE));
  }

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

  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;

  callback.Run(status);
}

void SyncWorker::MaybeStartFetchChanges() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;
}

void SyncWorker::DidFetchChanges(SyncStatusCode status) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

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
      if (has_refresh_token_) {
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
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  RemoteServiceState old_state = GetCurrentState();
  service_state_ = state;

  if (old_state == GetCurrentState())
    return;

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Service state changed: %d->%d: %s",
            old_state, GetCurrentState(), description.c_str());

  FOR_EACH_OBSERVER(
      Observer, observers_,
      UpdateServiceState(GetCurrentState(), description));
}

drive::DriveServiceInterface* SyncWorker::GetDriveService() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return context_->GetDriveService();
}

drive::DriveUploaderInterface* SyncWorker::GetDriveUploader() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return context_->GetDriveUploader();
}

MetadataDatabase* SyncWorker::GetMetadataDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return context_->GetMetadataDatabase();
}

}  // namespace drive_backend
}  // namespace sync_file_system

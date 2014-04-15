// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include <vector>

#include "base/bind.h"
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
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_worker.h"
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

scoped_ptr<SyncEngine> SyncEngine::CreateForBrowserContext(
    content::BrowserContext* context) {
  scoped_refptr<base::SequencedWorkerPool> worker_pool(
      content::BrowserThread::GetBlockingPool());
  scoped_refptr<base::SequencedTaskRunner> drive_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  Profile* profile = Profile::FromBrowserContext(context);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  scoped_ptr<drive::DriveServiceInterface> drive_service(
      new drive::DriveAPIService(
          token_service,
          context->GetRequestContext(),
          drive_task_runner.get(),
          GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction),
          GURL(google_apis::DriveApiUrlGenerator::
               kBaseDownloadUrlForProduction),
          GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
          std::string() /* custom_user_agent */));
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  drive_service->Initialize(signin_manager->GetAuthenticatedAccountId());

  scoped_ptr<drive::DriveUploaderInterface> drive_uploader(
      new drive::DriveUploader(drive_service.get(), drive_task_runner.get()));

  drive::DriveNotificationManager* notification_manager =
      drive::DriveNotificationManagerFactory::GetForBrowserContext(context);
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(context)->extension_service();

  scoped_refptr<base::SequencedTaskRunner> task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  scoped_ptr<drive_backend::SyncEngine> sync_engine(
      new SyncEngine(drive_service.Pass(),
                     drive_uploader.Pass(),
                     notification_manager,
                     extension_service,
                     signin_manager));
  sync_engine->Initialize(GetSyncFileSystemDir(context->GetPath()),
                          task_runner.get(),
                          NULL);

  return sync_engine.Pass();
}

void SyncEngine::AppendDependsOnFactories(
    std::set<BrowserContextKeyedServiceFactory*>* factories) {
  DCHECK(factories);
  factories->insert(drive::DriveNotificationManagerFactory::GetInstance());
  factories->insert(SigninManagerFactory::GetInstance());
  factories->insert(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

SyncEngine::~SyncEngine() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  GetDriveService()->RemoveObserver(this);
  if (notification_manager_)
    notification_manager_->RemoveObserver(this);
}

void SyncEngine::Initialize(const base::FilePath& base_dir,
                            base::SequencedTaskRunner* task_runner,
                            leveldb::Env* env_override) {
  scoped_ptr<SyncEngineContext> sync_engine_context(
      new SyncEngineContext(drive_service_.get(),
                            drive_uploader_.get(),
                            task_runner));
  // TODO(peria): Move this create function to thread pool.
  sync_worker_ = SyncWorker::CreateOnWorker(weak_ptr_factory_.GetWeakPtr(),
                                            base_dir,
                                            sync_engine_context.Pass(),
                                            env_override);

  if (notification_manager_)
    notification_manager_->AddObserver(this);
  GetDriveService()->AddObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

void SyncEngine::AddServiceObserver(SyncServiceObserver* observer) {
  service_observers_.AddObserver(observer);
}

void SyncEngine::AddFileStatusObserver(FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void SyncEngine::RegisterOrigin(
    const GURL& origin, const SyncStatusCallback& callback) {
  sync_worker_->RegisterOrigin(origin, callback);
}

void SyncEngine::EnableOrigin(
    const GURL& origin, const SyncStatusCallback& callback) {
  sync_worker_->EnableOrigin(origin, callback);
}

void SyncEngine::DisableOrigin(
    const GURL& origin, const SyncStatusCallback& callback) {
  sync_worker_->DisableOrigin(origin, callback);
}

void SyncEngine::UninstallOrigin(
    const GURL& origin,
    UninstallFlag flag,
    const SyncStatusCallback& callback) {
  sync_worker_->UninstallOrigin(origin, flag, callback);
}

void SyncEngine::ProcessRemoteChange(const SyncFileCallback& callback) {
  sync_worker_->ProcessRemoteChange(callback);
}

void SyncEngine::SetRemoteChangeProcessor(
    RemoteChangeProcessor* processor) {
  sync_worker_->SetRemoteChangeProcessor(processor);
}

LocalChangeProcessor* SyncEngine::GetLocalChangeProcessor() {
  return this;
}

bool SyncEngine::IsConflicting(const fileapi::FileSystemURL& url) {
  // TODO(tzik): Implement this before we support manual conflict resolution.
  return false;
}

RemoteServiceState SyncEngine::GetCurrentState() const {
  return sync_worker_->GetCurrentState();
}

void SyncEngine::GetOriginStatusMap(OriginStatusMap* status_map) {
  sync_worker_->GetOriginStatusMap(status_map);
}

scoped_ptr<base::ListValue> SyncEngine::DumpFiles(const GURL& origin) {
  return sync_worker_->DumpFiles(origin);
}

scoped_ptr<base::ListValue> SyncEngine::DumpDatabase() {
  return sync_worker_->DumpDatabase();
}

void SyncEngine::SetSyncEnabled(bool enabled) {
  sync_worker_->SetSyncEnabled(enabled);
}

void SyncEngine::UpdateSyncEnabled(bool enabled) {
  const char* status_message = enabled ? "Sync is enabled" : "Sync is disabled";
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), status_message));
}

SyncStatusCode SyncEngine::SetDefaultConflictResolutionPolicy(
    ConflictResolutionPolicy policy) {
  return sync_worker_->SetDefaultConflictResolutionPolicy(policy);
}

SyncStatusCode SyncEngine::SetConflictResolutionPolicy(
    const GURL& origin,
    ConflictResolutionPolicy policy) {
  return sync_worker_->SetConflictResolutionPolicy(origin, policy);
}

ConflictResolutionPolicy SyncEngine::GetDefaultConflictResolutionPolicy()
    const {
  return sync_worker_->GetDefaultConflictResolutionPolicy();
}

ConflictResolutionPolicy SyncEngine::GetConflictResolutionPolicy(
    const GURL& origin) const {
  return sync_worker_->GetConflictResolutionPolicy(origin);
}

void SyncEngine::GetRemoteVersions(
    const fileapi::FileSystemURL& url,
    const RemoteVersionsCallback& callback) {
  // TODO(tzik): Implement this before we support manual conflict resolution.
  callback.Run(SYNC_STATUS_FAILED, std::vector<Version>());
}

void SyncEngine::DownloadRemoteVersion(
    const fileapi::FileSystemURL& url,
    const std::string& version_id,
    const DownloadVersionCallback& callback) {
  // TODO(tzik): Implement this before we support manual conflict resolution.
  callback.Run(SYNC_STATUS_FAILED, webkit_blob::ScopedFile());
}

void SyncEngine::PromoteDemotedChanges() {
  MetadataDatabase* metadata_db = GetMetadataDatabase();
  if (metadata_db && metadata_db->HasLowPriorityDirtyTracker()) {
    metadata_db->PromoteLowerPriorityTrackersToNormal();
    FOR_EACH_OBSERVER(
        Observer,
        service_observers_,
        OnRemoteChangeQueueUpdated(metadata_db->CountDirtyTracker()));
  }
}

void SyncEngine::ApplyLocalChange(
    const FileChange& local_change,
    const base::FilePath& local_path,
    const SyncFileMetadata& local_metadata,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  sync_worker_->ApplyLocalChange(
      local_change, local_path, local_metadata, url, callback);
}

SyncTaskManager* SyncEngine::GetSyncTaskManagerForTesting() {
  return sync_worker_->GetSyncTaskManager();
}

void SyncEngine::OnNotificationReceived() {
  sync_worker_->OnNotificationReceived();
}

void SyncEngine::OnPushNotificationEnabled(bool) {}

void SyncEngine::OnReadyToSendRequests() {
  sync_worker_->OnReadyToSendRequests(
      signin_manager_ ? signin_manager_->GetAuthenticatedAccountId() : "");
}

void SyncEngine::OnRefreshTokenInvalid() {
  sync_worker_->OnRefreshTokenInvalid();
}

void SyncEngine::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  sync_worker_->OnNetworkChanged(type);
}

drive::DriveServiceInterface* SyncEngine::GetDriveService() {
  return sync_worker_->GetDriveService();
}

drive::DriveUploaderInterface* SyncEngine::GetDriveUploader() {
  return sync_worker_->GetDriveUploader();
}

MetadataDatabase* SyncEngine::GetMetadataDatabase() {
  return sync_worker_->GetMetadataDatabase();
}

SyncEngine::SyncEngine(
    scoped_ptr<drive::DriveServiceInterface> drive_service,
    scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
    drive::DriveNotificationManager* notification_manager,
    ExtensionServiceInterface* extension_service,
    SigninManagerBase* signin_manager)
    : drive_service_(drive_service.Pass()),
      drive_uploader_(drive_uploader.Pass()),
      notification_manager_(notification_manager),
      extension_service_(extension_service),
      signin_manager_(signin_manager),
      weak_ptr_factory_(this) {}

void SyncEngine::DidProcessRemoteChange(RemoteToLocalSyncer* syncer) {
  if (syncer->sync_action() != SYNC_ACTION_NONE && syncer->url().is_valid()) {
    FOR_EACH_OBSERVER(FileStatusObserver,
                      file_status_observers_,
                      OnFileStatusChanged(syncer->url(),
                                          SYNC_FILE_STATUS_SYNCED,
                                          syncer->sync_action(),
                                          SYNC_DIRECTION_REMOTE_TO_LOCAL));
  }
}

void SyncEngine::DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                                     SyncStatusCode status) {
  if ((status == SYNC_STATUS_OK || status == SYNC_STATUS_RETRY) &&
      syncer->url().is_valid() &&
      syncer->sync_action() != SYNC_ACTION_NONE) {
    fileapi::FileSystemURL updated_url = syncer->url();
    if (!syncer->target_path().empty()) {
      updated_url = CreateSyncableFileSystemURL(syncer->url().origin(),
                                                syncer->target_path());
    }
    FOR_EACH_OBSERVER(FileStatusObserver,
                      file_status_observers_,
                      OnFileStatusChanged(updated_url,
                                          SYNC_FILE_STATUS_SYNCED,
                                          syncer->sync_action(),
                                          SYNC_DIRECTION_LOCAL_TO_REMOTE));
  }
}

void SyncEngine::UpdateServiceState(const std::string& description) {
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), description));
}

void SyncEngine::UpdateRegisteredApps() {
  if (!extension_service_)
    return;

  MetadataDatabase* metadata_db = GetMetadataDatabase();
  DCHECK(metadata_db);
  std::vector<std::string> app_ids;
  metadata_db->GetRegisteredAppIDs(&app_ids);

  // Update the status of every origin using status from ExtensionService.
  for (std::vector<std::string>::const_iterator itr = app_ids.begin();
       itr != app_ids.end(); ++itr) {
    const std::string& app_id = *itr;
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(app_id);
    if (!extension_service_->GetInstalledExtension(app_id)) {
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
    bool is_app_enabled = extension_service_->IsExtensionEnabled(app_id);
    bool is_app_root_tracker_enabled =
        tracker.tracker_kind() == TRACKER_KIND_APP_ROOT;
    if (is_app_enabled && !is_app_root_tracker_enabled)
      EnableOrigin(origin, base::Bind(&EmptyStatusCallback));
    else if (!is_app_enabled && is_app_root_tracker_enabled)
      DisableOrigin(origin, base::Bind(&EmptyStatusCallback));
  }
}

void SyncEngine::NotifyLastOperationStatus() {
  FOR_EACH_OBSERVER(
      Observer,
      service_observers_,
      OnRemoteChangeQueueUpdated(
          GetMetadataDatabase()->CountDirtyTracker()));
}

}  // namespace drive_backend
}  // namespace sync_file_system

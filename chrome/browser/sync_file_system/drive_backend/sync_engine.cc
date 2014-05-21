// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
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
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_service_on_worker.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_service_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_uploader_on_worker.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_uploader_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_on_worker.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_wrapper.h"
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

class SyncEngine::WorkerObserver : public SyncWorker::Observer {
 public:
  WorkerObserver(base::SequencedTaskRunner* ui_task_runner,
                 base::WeakPtr<SyncEngine> sync_engine)
      : ui_task_runner_(ui_task_runner),
        sync_engine_(sync_engine) {}

  virtual ~WorkerObserver() {}

  virtual void OnPendingFileListUpdated(int item_count) OVERRIDE {
    if (ui_task_runner_->RunsTasksOnCurrentThread()) {
      sync_engine_->OnPendingFileListUpdated(item_count);
      return;
    }

    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SyncEngine::OnPendingFileListUpdated,
                   sync_engine_,
                   item_count));
  }

  virtual void OnFileStatusChanged(const fileapi::FileSystemURL& url,
                                   SyncFileStatus file_status,
                                   SyncAction sync_action,
                                   SyncDirection direction) OVERRIDE {
    if (ui_task_runner_->RunsTasksOnCurrentThread()) {
      sync_engine_->OnFileStatusChanged(
          url, file_status, sync_action, direction);
      return;
    }

    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SyncEngine::OnFileStatusChanged,
                   sync_engine_,
                   url, file_status, sync_action, direction));
  }

  virtual void UpdateServiceState(RemoteServiceState state,
                                  const std::string& description) OVERRIDE {
    if (ui_task_runner_->RunsTasksOnCurrentThread()) {
      sync_engine_->UpdateServiceState(state, description);
      return;
    }

    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SyncEngine::UpdateServiceState,
                   sync_engine_, state, description));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  base::WeakPtr<SyncEngine> sync_engine_;

  DISALLOW_COPY_AND_ASSIGN(WorkerObserver);
};

namespace {

void EmptyStatusCallback(SyncStatusCode status) {}

void DidRegisterOrigin(const base::TimeTicks& start_time,
                       const SyncStatusCallback& callback,
                       SyncStatusCode status) {
  base::TimeDelta delta(base::TimeTicks::Now() - start_time);
  HISTOGRAM_TIMES("SyncFileSystem.RegisterOriginTime", delta);
  callback.Run(status);
}

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

  scoped_refptr<base::SequencedTaskRunner> file_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  // TODO(peria): Create another task runner to manage SyncWorker.
  scoped_refptr<base::SingleThreadTaskRunner>
      worker_task_runner = base::MessageLoopProxy::current();

  scoped_ptr<drive_backend::SyncEngine> sync_engine(
      new SyncEngine(drive_service.Pass(),
                     drive_uploader.Pass(),
                     worker_task_runner,
                     notification_manager,
                     extension_service,
                     signin_manager));
  sync_engine->Initialize(GetSyncFileSystemDir(context->GetPath()),
                          file_task_runner.get(),
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

  // TODO(tzik): Destroy |sync_worker_| and |worker_observer_| on the worker.
}

void SyncEngine::Initialize(const base::FilePath& base_dir,
                            base::SequencedTaskRunner* file_task_runner,
                            leveldb::Env* env_override) {
  // DriveServiceWrapper and DriveServiceOnWorker relay communications
  // between DriveService and syncers in SyncWorker.
  scoped_ptr<drive::DriveServiceInterface>
      drive_service_on_worker(
          new DriveServiceOnWorker(drive_service_wrapper_->AsWeakPtr(),
                                   base::MessageLoopProxy::current(),
                                   worker_task_runner_));
  scoped_ptr<drive::DriveUploaderInterface>
      drive_uploader_on_worker(
          new DriveUploaderOnWorker(drive_uploader_wrapper_->AsWeakPtr(),
                                    base::MessageLoopProxy::current(),
                                    worker_task_runner_));
  scoped_ptr<SyncEngineContext>
      sync_engine_context(
          new SyncEngineContext(drive_service_on_worker.Pass(),
                                drive_uploader_on_worker.Pass(),
                                base::MessageLoopProxy::current(),
                                worker_task_runner_,
                                file_task_runner));

  worker_observer_.reset(
      new WorkerObserver(base::MessageLoopProxy::current(),
                         weak_ptr_factory_.GetWeakPtr()));

  base::WeakPtr<ExtensionServiceInterface> extension_service_weak_ptr;
  if (extension_service_)
    extension_service_weak_ptr = extension_service_->AsWeakPtr();

  // TODO(peria): Use PostTask on |worker_task_runner_| to call this function.
  sync_worker_ = SyncWorker::CreateOnWorker(
      base_dir,
      worker_observer_.get(),
      extension_service_weak_ptr,
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
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::RegisterOrigin,
                 base::Unretained(sync_worker_.get()),
                 origin,
                 RelayCallbackToCurrentThread(
                     FROM_HERE,
                     base::Bind(&DidRegisterOrigin,
                                base::TimeTicks::Now(),
                                callback))));
}

void SyncEngine::EnableOrigin(
    const GURL& origin, const SyncStatusCallback& callback) {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::EnableOrigin,
                 base::Unretained(sync_worker_.get()),
                 origin,
                 RelayCallbackToCurrentThread(
                     FROM_HERE, callback)));
}

void SyncEngine::DisableOrigin(
    const GURL& origin, const SyncStatusCallback& callback) {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::DisableOrigin,
                 base::Unretained(sync_worker_.get()),
                 origin,
                 RelayCallbackToCurrentThread(
                     FROM_HERE, callback)));
}

void SyncEngine::UninstallOrigin(
    const GURL& origin,
    UninstallFlag flag,
    const SyncStatusCallback& callback) {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::UninstallOrigin,
                 base::Unretained(sync_worker_.get()),
                 origin, flag,
                 RelayCallbackToCurrentThread(
                     FROM_HERE, callback)));
}

void SyncEngine::ProcessRemoteChange(const SyncFileCallback& callback) {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::ProcessRemoteChange,
                 base::Unretained(sync_worker_.get()),
                 RelayCallbackToCurrentThread(
                     FROM_HERE, callback)));
}

void SyncEngine::SetRemoteChangeProcessor(RemoteChangeProcessor* processor) {
  remote_change_processor_ = processor;
  remote_change_processor_wrapper_.reset(
      new RemoteChangeProcessorWrapper(processor));

  remote_change_processor_on_worker_.reset(new RemoteChangeProcessorOnWorker(
      remote_change_processor_wrapper_->AsWeakPtr(),
      base::MessageLoopProxy::current(), /* ui_task_runner */
      worker_task_runner_));

  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::SetRemoteChangeProcessor,
                 base::Unretained(sync_worker_.get()),
                 remote_change_processor_on_worker_.get()));
}

LocalChangeProcessor* SyncEngine::GetLocalChangeProcessor() {
  return this;
}

RemoteServiceState SyncEngine::GetCurrentState() const {
  return service_state_;
}

void SyncEngine::GetOriginStatusMap(OriginStatusMap* status_map) {
  // TODO(peria): Make this route asynchronous.
  sync_worker_->GetOriginStatusMap(status_map);
}

void SyncEngine::DumpFiles(const GURL& origin,
                           const ListCallback& callback) {
  PostTaskAndReplyWithResult(
      worker_task_runner_,
      FROM_HERE,
      base::Bind(&SyncWorker::DumpFiles,
                 base::Unretained(sync_worker_.get()),
                 origin),
      callback);
}

void SyncEngine::DumpDatabase(const ListCallback& callback) {
  PostTaskAndReplyWithResult(
      worker_task_runner_,
      FROM_HERE,
      base::Bind(&SyncWorker::DumpDatabase,
                 base::Unretained(sync_worker_.get())),
      callback);
}

void SyncEngine::SetSyncEnabled(bool enabled) {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::SetSyncEnabled,
                 base::Unretained(sync_worker_.get()),
                 enabled));
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
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::ApplyLocalChange,
                 base::Unretained(sync_worker_.get()),
                 local_change,
                 local_path,
                 local_metadata,
                 url,
                 RelayCallbackToCurrentThread(
                     FROM_HERE, callback)));
}

SyncTaskManager* SyncEngine::GetSyncTaskManagerForTesting() {
  return sync_worker_->GetSyncTaskManager();
}

void SyncEngine::OnNotificationReceived() {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::OnNotificationReceived,
                 base::Unretained(sync_worker_.get())));
}

void SyncEngine::OnPushNotificationEnabled(bool) {}

void SyncEngine::OnReadyToSendRequests() {
  const std::string account_id =
      signin_manager_ ? signin_manager_->GetAuthenticatedAccountId() : "";

  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::OnReadyToSendRequests,
                 base::Unretained(sync_worker_.get()),
                 account_id));
}

void SyncEngine::OnRefreshTokenInvalid() {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::OnRefreshTokenInvalid,
                 base::Unretained(sync_worker_.get())));
}

void SyncEngine::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SyncWorker::OnNetworkChanged,
                 base::Unretained(sync_worker_.get()),
                 type));
}

drive::DriveServiceInterface* SyncEngine::GetDriveService() {
  return drive_service_.get();
}

drive::DriveUploaderInterface* SyncEngine::GetDriveUploader() {
  return drive_uploader_.get();
}

MetadataDatabase* SyncEngine::GetMetadataDatabase() {
  // TODO(peria): Post task
  return sync_worker_->GetMetadataDatabase();
}

SyncEngine::SyncEngine(
    scoped_ptr<drive::DriveServiceInterface> drive_service,
    scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
    base::SequencedTaskRunner* worker_task_runner,
    drive::DriveNotificationManager* notification_manager,
    ExtensionServiceInterface* extension_service,
    SigninManagerBase* signin_manager)
    : drive_service_(drive_service.Pass()),
      drive_service_wrapper_(new DriveServiceWrapper(drive_service_.get())),
      drive_uploader_(drive_uploader.Pass()),
      drive_uploader_wrapper_(new DriveUploaderWrapper(drive_uploader_.get())),
      service_state_(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE),
      notification_manager_(notification_manager),
      extension_service_(extension_service),
      signin_manager_(signin_manager),
      worker_task_runner_(worker_task_runner),
      weak_ptr_factory_(this) {}

void SyncEngine::OnPendingFileListUpdated(int item_count) {
  FOR_EACH_OBSERVER(
      Observer,
      service_observers_,
      OnRemoteChangeQueueUpdated(item_count));
}

void SyncEngine::OnFileStatusChanged(const fileapi::FileSystemURL& url,
                                     SyncFileStatus file_status,
                                     SyncAction sync_action,
                                     SyncDirection direction) {
  FOR_EACH_OBSERVER(FileStatusObserver,
                    file_status_observers_,
                    OnFileStatusChanged(
                        url, file_status, sync_action, direction));
}

void SyncEngine::UpdateServiceState(RemoteServiceState state,
                                    const std::string& description) {
  service_state_ = state;

  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(state, description));
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

}  // namespace drive_backend
}  // namespace sync_file_system

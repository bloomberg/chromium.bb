// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/drive/drive_notification_observer.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_tracker.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "net/base/network_change_notifier.h"

class ExtensionServiceInterface;
class OAuth2TokenService;

namespace base {
class SequencedTaskRunner;
}

namespace drive {
class DriveServiceInterface;
class DriveNotificationManager;
class DriveUploaderInterface;
}

namespace leveldb {
class Env;
}

namespace net {
class URLRequestContextGetter;
}

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

class DriveServiceWrapper;
class DriveUploaderWrapper;
class MetadataDatabase;
class RemoteChangeProcessorOnWorker;
class RemoteChangeProcessorWrapper;
class SyncTaskManager;
class SyncWorkerInterface;

class SyncEngine : public RemoteFileSyncService,
                   public LocalChangeProcessor,
                   public drive::DriveNotificationObserver,
                   public drive::DriveServiceObserver,
                   public net::NetworkChangeNotifier::NetworkChangeObserver,
                   public SigninManagerBase::Observer {
 public:
  typedef RemoteFileSyncService::Observer SyncServiceObserver;

  class DriveServiceFactory {
   public:
    DriveServiceFactory() {}
    virtual ~DriveServiceFactory() {}
    virtual scoped_ptr<drive::DriveServiceInterface> CreateDriveService(
        OAuth2TokenService* oauth2_token_service,
        net::URLRequestContextGetter* url_request_context_getter,
        base::SequencedTaskRunner* blocking_task_runner);

   private:
    DISALLOW_COPY_AND_ASSIGN(DriveServiceFactory);
  };

  static scoped_ptr<SyncEngine> CreateForBrowserContext(
      content::BrowserContext* context,
      TaskLogger* task_logger);
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  virtual ~SyncEngine();
  void Reset();

  // Can be called more than once.
  void Initialize();

  void InitializeForTesting(
      scoped_ptr<drive::DriveServiceInterface> drive_service,
      scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
      scoped_ptr<SyncWorkerInterface> sync_worker);
  void InitializeInternal(
      scoped_ptr<drive::DriveServiceInterface> drive_service,
      scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
      scoped_ptr<SyncWorkerInterface> sync_worker);

  // RemoteFileSyncService overrides.
  virtual void AddServiceObserver(SyncServiceObserver* observer) OVERRIDE;
  virtual void AddFileStatusObserver(FileStatusObserver* observer) OVERRIDE;
  virtual void RegisterOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void EnableOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void DisableOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void UninstallOrigin(
      const GURL& origin,
      UninstallFlag flag,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void ProcessRemoteChange(const SyncFileCallback& callback) OVERRIDE;
  virtual void SetRemoteChangeProcessor(
      RemoteChangeProcessor* processor) OVERRIDE;
  virtual LocalChangeProcessor* GetLocalChangeProcessor() OVERRIDE;
  virtual RemoteServiceState GetCurrentState() const OVERRIDE;
  virtual void GetOriginStatusMap(const StatusMapCallback& callback) OVERRIDE;
  virtual void DumpFiles(const GURL& origin,
                         const ListCallback& callback) OVERRIDE;
  virtual void DumpDatabase(const ListCallback& callback) OVERRIDE;
  virtual void SetSyncEnabled(bool enabled) OVERRIDE;
  virtual void PromoteDemotedChanges(const base::Closure& callback) OVERRIDE;

  // LocalChangeProcessor overrides.
  virtual void ApplyLocalChange(
      const FileChange& local_change,
      const base::FilePath& local_path,
      const SyncFileMetadata& local_metadata,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback) OVERRIDE;

  // drive::DriveNotificationObserver overrides.
  virtual void OnNotificationReceived() OVERRIDE;
  virtual void OnPushNotificationEnabled(bool enabled) OVERRIDE;

  // drive::DriveServiceObserver overrides.
  virtual void OnReadyToSendRequests() OVERRIDE;
  virtual void OnRefreshTokenInvalid() OVERRIDE;

  // net::NetworkChangeNotifier::NetworkChangeObserver overrides.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // SigninManagerBase::Observer overrides.
  virtual void GoogleSigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

 private:
  class WorkerObserver;

  friend class DriveBackendSyncTest;
  friend class SyncEngineTest;

  SyncEngine(base::SingleThreadTaskRunner* ui_task_runner,
             base::SequencedTaskRunner* worker_task_runner,
             base::SequencedTaskRunner* drive_task_runner,
             const base::FilePath& sync_file_system_dir,
             TaskLogger* task_logger,
             drive::DriveNotificationManager* notification_manager,
             ExtensionServiceInterface* extension_service,
             SigninManagerBase* signin_manager,
             OAuth2TokenService* token_service,
             net::URLRequestContextGetter* request_context,
             scoped_ptr<DriveServiceFactory> drive_service_factory,
             leveldb::Env* env_override);

  // Called by WorkerObserver.
  void OnPendingFileListUpdated(int item_count);
  void OnFileStatusChanged(const fileapi::FileSystemURL& url,
                           SyncFileStatus file_status,
                           SyncAction sync_action,
                           SyncDirection direction);
  void UpdateServiceState(RemoteServiceState state,
                          const std::string& description);

  SyncStatusCallback TrackCallback(const SyncStatusCallback& callback);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> drive_task_runner_;

  const base::FilePath sync_file_system_dir_;
  TaskLogger* task_logger_;

  // These external services are not owned by SyncEngine.
  // The owner of the SyncEngine is responsible for their lifetime.
  // I.e. the owner should declare the dependency explicitly by calling
  // KeyedService::DependsOn().
  drive::DriveNotificationManager* notification_manager_;
  ExtensionServiceInterface* extension_service_;
  SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;

  scoped_refptr<net::URLRequestContextGetter> request_context_;

  scoped_ptr<DriveServiceFactory> drive_service_factory_;

  scoped_ptr<drive::DriveServiceInterface> drive_service_;
  scoped_ptr<DriveServiceWrapper> drive_service_wrapper_;
  scoped_ptr<drive::DriveUploaderInterface> drive_uploader_;
  scoped_ptr<DriveUploaderWrapper> drive_uploader_wrapper_;

  RemoteChangeProcessor* remote_change_processor_;  // Not owned.
  scoped_ptr<RemoteChangeProcessorWrapper> remote_change_processor_wrapper_;
  // Delete this on worker.
  scoped_ptr<RemoteChangeProcessorOnWorker> remote_change_processor_on_worker_;

  RemoteServiceState service_state_;
  bool has_refresh_token_;
  bool network_available_;
  bool sync_enabled_;

  // Delete them on worker.
  scoped_ptr<WorkerObserver> worker_observer_;
  scoped_ptr<SyncWorkerInterface> sync_worker_;

  ObserverList<SyncServiceObserver> service_observers_;
  ObserverList<FileStatusObserver> file_status_observers_;
  leveldb::Env* env_override_;

  CallbackTracker callback_tracker_;

  base::WeakPtrFactory<SyncEngine> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SyncEngine);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_

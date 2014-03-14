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
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "net/base/network_change_notifier.h"

class ExtensionServiceInterface;
class SigninManagerBase;

namespace base {
class SequencedTaskRunner;
}

namespace drive {
class DriveServiceInterface;
class DriveNotificationManager;
}

namespace leveldb {
class Env;
}

namespace sync_file_system {
namespace drive_backend {

class LocalToRemoteSyncer;
class MetadataDatabase;
class RemoteToLocalSyncer;
class SyncEngineInitializer;

class SyncEngine : public RemoteFileSyncService,
                   public LocalChangeProcessor,
                   public SyncTaskManager::Client,
                   public drive::DriveNotificationObserver,
                   public drive::DriveServiceObserver,
                   public net::NetworkChangeNotifier::NetworkChangeObserver,
                   public SyncEngineContext {
 public:
  typedef Observer SyncServiceObserver;

  static scoped_ptr<SyncEngine> CreateForBrowserContext(
      content::BrowserContext* context);
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  virtual ~SyncEngine();

  void Initialize();

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
  virtual bool IsConflicting(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual RemoteServiceState GetCurrentState() const OVERRIDE;
  virtual void GetOriginStatusMap(OriginStatusMap* status_map) OVERRIDE;
  virtual scoped_ptr<base::ListValue> DumpFiles(const GURL& origin) OVERRIDE;
  virtual scoped_ptr<base::ListValue> DumpDatabase() OVERRIDE;
  virtual void SetSyncEnabled(bool enabled) OVERRIDE;
  virtual SyncStatusCode SetDefaultConflictResolutionPolicy(
      ConflictResolutionPolicy policy) OVERRIDE;
  virtual SyncStatusCode SetConflictResolutionPolicy(
      const GURL& origin,
      ConflictResolutionPolicy policy) OVERRIDE;
  virtual ConflictResolutionPolicy GetDefaultConflictResolutionPolicy()
      const OVERRIDE;
  virtual ConflictResolutionPolicy GetConflictResolutionPolicy(
      const GURL& origin) const OVERRIDE;
  virtual void GetRemoteVersions(
      const fileapi::FileSystemURL& url,
      const RemoteVersionsCallback& callback) OVERRIDE;
  virtual void DownloadRemoteVersion(
      const fileapi::FileSystemURL& url,
      const std::string& version_id,
      const DownloadVersionCallback& callback) OVERRIDE;
  virtual void PromoteDemotedChanges() OVERRIDE;

  // LocalChangeProcessor overrides.
  virtual void ApplyLocalChange(
      const FileChange& local_change,
      const base::FilePath& local_path,
      const SyncFileMetadata& local_metadata,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback) OVERRIDE;

  // SyncTaskManager::Client overrides.
  virtual void MaybeScheduleNextTask() OVERRIDE;
  virtual void NotifyLastOperationStatus(SyncStatusCode sync_status,
                                         bool used_network) OVERRIDE;

  // drive::DriveNotificationObserver overrides.
  virtual void OnNotificationReceived() OVERRIDE;
  virtual void OnPushNotificationEnabled(bool enabled) OVERRIDE;

  // drive::DriveServiceObserver overrides.
  virtual void OnReadyToSendRequests() OVERRIDE;
  virtual void OnRefreshTokenInvalid() OVERRIDE;

  // net::NetworkChangeNotifier::NetworkChangeObserver overrides.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // SyncEngineContext overrides.
  virtual drive::DriveServiceInterface* GetDriveService() OVERRIDE;
  virtual drive::DriveUploaderInterface* GetDriveUploader() OVERRIDE;
  virtual MetadataDatabase* GetMetadataDatabase() OVERRIDE;
  virtual RemoteChangeProcessor* GetRemoteChangeProcessor() OVERRIDE;
  virtual base::SequencedTaskRunner* GetBlockingTaskRunner() OVERRIDE;

 private:
  friend class DriveBackendSyncTest;
  friend class SyncEngineTest;

  SyncEngine(const base::FilePath& base_dir,
             base::SequencedTaskRunner* task_runner,
             scoped_ptr<drive::DriveServiceInterface> drive_service,
             scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
             drive::DriveNotificationManager* notification_manager,
             ExtensionServiceInterface* extension_service,
             SigninManagerBase* signin_manager,
             leveldb::Env* env_override);

  void DoDisableApp(const std::string& app_id,
                    const SyncStatusCallback& callback);
  void DoEnableApp(const std::string& app_id,
                   const SyncStatusCallback& callback);

  void PostInitializeTask();
  void DidInitialize(SyncEngineInitializer* initializer,
                     SyncStatusCode status);
  void DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                              const SyncFileCallback& callback,
                              SyncStatusCode status);
  void DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                           const SyncStatusCallback& callback,
                           SyncStatusCode status);

  void MaybeStartFetchChanges();
  void DidResolveConflict(SyncStatusCode status);
  void DidFetchChanges(SyncStatusCode status);

  void UpdateServiceStateFromSyncStatusCode(SyncStatusCode state,
                                            bool used_network);
  void UpdateServiceState(RemoteServiceState state,
                          const std::string& description);
  void UpdateRegisteredApps();

  base::FilePath base_dir_;
  base::FilePath temporary_file_dir_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  leveldb::Env* env_override_;

  scoped_ptr<drive::DriveServiceInterface> drive_service_;
  scoped_ptr<drive::DriveUploaderInterface> drive_uploader_;
  scoped_ptr<MetadataDatabase> metadata_database_;

  // These external services are not owned by SyncEngine.
  // The owner of the SyncEngine is responsible for their lifetime.
  // I.e. the owner should declare the dependency explicitly by calling
  // KeyedService::DependsOn().
  drive::DriveNotificationManager* notification_manager_;
  ExtensionServiceInterface* extension_service_;
  SigninManagerBase* signin_manager_;

  ObserverList<SyncServiceObserver> service_observers_;
  ObserverList<FileStatusObserver> file_status_observers_;
  RemoteChangeProcessor* remote_change_processor_;

  RemoteServiceState service_state_;

  bool should_check_conflict_;
  bool should_check_remote_change_;
  bool listing_remote_changes_;
  base::TimeTicks time_to_check_changes_;

  bool sync_enabled_;
  ConflictResolutionPolicy default_conflict_resolution_policy_;
  bool network_available_;

  scoped_ptr<SyncTaskManager> task_manager_;

  base::WeakPtrFactory<SyncEngine> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngine);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_

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
class DriveUploaderInterface;
}

namespace leveldb {
class Env;
}

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

class LocalToRemoteSyncer;
class MetadataDatabase;
class RemoteToLocalSyncer;
class SyncEngineInitializer;
class SyncTaskManager;
class SyncWorker;

class SyncEngine : public RemoteFileSyncService,
                   public LocalChangeProcessor,
                   public drive::DriveNotificationObserver,
                   public drive::DriveServiceObserver,
                   public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  typedef Observer SyncServiceObserver;

  static scoped_ptr<SyncEngine> CreateForBrowserContext(
      content::BrowserContext* context);
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  virtual ~SyncEngine();

  void Initialize(
      const base::FilePath& base_dir,
      base::SequencedTaskRunner* task_runner,
      leveldb::Env* env_override);

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

  // drive::DriveNotificationObserver overrides.
  virtual void OnNotificationReceived() OVERRIDE;
  virtual void OnPushNotificationEnabled(bool enabled) OVERRIDE;

  // drive::DriveServiceObserver overrides.
  virtual void OnReadyToSendRequests() OVERRIDE;
  virtual void OnRefreshTokenInvalid() OVERRIDE;

  // net::NetworkChangeNotifier::NetworkChangeObserver overrides.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  drive::DriveServiceInterface* GetDriveService();
  drive::DriveUploaderInterface* GetDriveUploader();
  MetadataDatabase* GetMetadataDatabase();
  SyncTaskManager* GetSyncTaskManagerForTesting();

  // Notifies update of sync status to each observer.
  void UpdateSyncEnabled(bool enabled);

 private:
  friend class DriveBackendSyncTest;
  friend class SyncEngineTest;
  // TODO(peria): Remove friendship with SyncWorker
  friend class SyncWorker;

  SyncEngine(scoped_ptr<drive::DriveServiceInterface> drive_service,
             scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
             drive::DriveNotificationManager* notification_manager,
             ExtensionServiceInterface* extension_service,
             SigninManagerBase* signin_manager);

  void DidProcessRemoteChange(RemoteToLocalSyncer* syncer);
  void DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                           SyncStatusCode status);
  void UpdateServiceState(const std::string& description);
  void UpdateRegisteredApps();
  void NotifyLastOperationStatus();

  scoped_ptr<drive::DriveServiceInterface> drive_service_;
  scoped_ptr<drive::DriveUploaderInterface> drive_uploader_;

  // These external services are not owned by SyncEngine.
  // The owner of the SyncEngine is responsible for their lifetime.
  // I.e. the owner should declare the dependency explicitly by calling
  // KeyedService::DependsOn().
  drive::DriveNotificationManager* notification_manager_;
  ExtensionServiceInterface* extension_service_;
  SigninManagerBase* signin_manager_;

  ObserverList<SyncServiceObserver> service_observers_;
  ObserverList<FileStatusObserver> file_status_observers_;

  scoped_ptr<SyncWorker> sync_worker_;

  base::WeakPtrFactory<SyncEngine> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SyncEngine);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_

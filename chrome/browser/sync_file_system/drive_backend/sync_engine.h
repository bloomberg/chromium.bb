// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/drive/drive_notification_observer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_task_manager.h"

class ExtensionService;

namespace base {
class SequencedTaskRunner;
}

namespace drive {
class DriveAPIService;
class DriveNotificationManager;
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
                   public SyncEngineContext {
 public:
  typedef Observer SyncServiceObserver;

  SyncEngine(const base::FilePath& base_dir,
             base::SequencedTaskRunner* task_runner,
             scoped_ptr<drive::DriveAPIService> drive_service,
             drive::DriveNotificationManager* notification_manager,
             ExtensionService* extension_service);
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
  virtual void SetSyncEnabled(bool enabled) OVERRIDE;
  virtual SyncStatusCode SetConflictResolutionPolicy(
      ConflictResolutionPolicy policy) OVERRIDE;
  virtual ConflictResolutionPolicy GetConflictResolutionPolicy() const OVERRIDE;
  virtual void GetRemoteVersions(
      const fileapi::FileSystemURL& url,
      const RemoteVersionsCallback& callback) OVERRIDE;
  virtual void DownloadRemoteVersion(
      const fileapi::FileSystemURL& url,
      const std::string& version_id,
      const DownloadVersionCallback& callback) OVERRIDE;

  // LocalChangeProcessor overrides.
  virtual void ApplyLocalChange(
      const FileChange& change,
      const base::FilePath& local_file_path,
      const SyncFileMetadata& local_file_metadata,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback) OVERRIDE;

  // SyncTaskManager::Client overrides.
  virtual void MaybeScheduleNextTask() OVERRIDE;
  virtual void NotifyLastOperationStatus(SyncStatusCode sync_status) OVERRIDE;

  // drive::DriveNotificationObserver overrides.
  virtual void OnNotificationReceived() OVERRIDE;
  virtual void OnPushNotificationEnabled(bool enabled) OVERRIDE;

  // SyncEngineContext overrides.
  virtual drive::DriveServiceInterface* GetDriveService() OVERRIDE;
  virtual MetadataDatabase* GetMetadataDatabase() OVERRIDE;

 private:
  void DoDisableApp(const std::string& app_id,
                    const SyncStatusCallback& callback);
  void DoEnableApp(const std::string& app_id,
                   const SyncStatusCallback& callback);

  void DidInitialize(SyncEngineInitializer* initializer,
                     SyncStatusCode status);
  void DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                              const SyncFileCallback& callback,
                              SyncStatusCode status);
  void DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                           const SyncStatusCallback& callback,
                           SyncStatusCode status);

  base::FilePath base_dir_;
  base::FilePath temporary_file_dir_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_ptr<drive::DriveAPIService> drive_service_;
  scoped_ptr<MetadataDatabase> metadata_database_;

  // These external services are not owned by SyncEngine.
  // The owner of the SyncEngine is responsible for their lifetime.
  // I.e. the owner should declare the dependency explicitly by calling
  // BrowserContextKeyedService::DependsOn().
  drive::DriveNotificationManager* notification_manager_;
  ExtensionService* extension_service_;

  ObserverList<SyncServiceObserver> service_observers_;
  ObserverList<FileStatusObserver> file_status_observers_;
  RemoteChangeProcessor* remote_change_processor_;

  SyncTaskManager task_manager_;
  base::WeakPtrFactory<SyncEngine> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngine);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_

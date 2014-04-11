// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/drive/drive_notification_observer.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "net/base/network_change_notifier.h"

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
class SyncEngine;
class SyncEngineContext;
class SyncEngineInitializer;

class SyncWorker : public SyncTaskManager::Client {
 public:

  static scoped_ptr<SyncWorker> CreateOnWorker(
      const base::WeakPtr<drive_backend::SyncEngine>& sync_engine,
      const base::FilePath& base_dir,
      scoped_ptr<drive::DriveServiceInterface> drive_service,
      scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
      base::SequencedTaskRunner* task_runner,
      leveldb::Env* env_override);

  virtual ~SyncWorker();

  void Initialize();

  // SyncTaskManager::Client overrides
  virtual void MaybeScheduleNextTask() OVERRIDE;
  virtual void NotifyLastOperationStatus(
      SyncStatusCode sync_status, bool used_network) OVERRIDE;

  void RegisterOrigin(const GURL& origin, const SyncStatusCallback& callback);
  void EnableOrigin(const GURL& origin, const SyncStatusCallback& callback);
  void DisableOrigin(const GURL& origin, const SyncStatusCallback& callback);
  void UninstallOrigin(
      const GURL& origin,
      RemoteFileSyncService::UninstallFlag flag,
      const SyncStatusCallback& callback);
  void ProcessRemoteChange(const SyncFileCallback& callback);
  void SetRemoteChangeProcessor(RemoteChangeProcessor* processor);
  RemoteServiceState GetCurrentState() const;
  void GetOriginStatusMap(RemoteFileSyncService::OriginStatusMap* status_map);
  scoped_ptr<base::ListValue> DumpFiles(const GURL& origin);
  scoped_ptr<base::ListValue> DumpDatabase();
  void SetSyncEnabled(bool enabled);
  SyncStatusCode SetDefaultConflictResolutionPolicy(
      ConflictResolutionPolicy policy);
  SyncStatusCode SetConflictResolutionPolicy(
      const GURL& origin,
      ConflictResolutionPolicy policy);
  ConflictResolutionPolicy GetDefaultConflictResolutionPolicy()
      const;
  ConflictResolutionPolicy GetConflictResolutionPolicy(
      const GURL& origin) const;

  void ApplyLocalChange(
      const FileChange& local_change,
      const base::FilePath& local_path,
      const SyncFileMetadata& local_metadata,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback);

  void OnNotificationReceived();

  void OnReadyToSendRequests(const std::string& account_id);
  void OnRefreshTokenInvalid();

  void OnNetworkChanged(net::NetworkChangeNotifier::ConnectionType type);

  drive::DriveServiceInterface* GetDriveService();
  drive::DriveUploaderInterface* GetDriveUploader();
  MetadataDatabase* GetMetadataDatabase();
  SyncTaskManager* GetSyncTaskManager();

 private:
  SyncWorker(const base::WeakPtr<drive_backend::SyncEngine>& sync_engine,
             const base::FilePath& base_dir,
             scoped_ptr<drive::DriveServiceInterface> drive_service,
             scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
             base::SequencedTaskRunner* task_runner,
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

  base::FilePath base_dir_;

  leveldb::Env* env_override_;

  // Sync with SyncEngine.
  RemoteServiceState service_state_;

  bool should_check_conflict_;
  bool should_check_remote_change_;
  bool listing_remote_changes_;
  base::TimeTicks time_to_check_changes_;

  bool sync_enabled_;
  ConflictResolutionPolicy default_conflict_resolution_policy_;
  bool network_available_;

  scoped_ptr<SyncTaskManager> task_manager_;

  scoped_ptr<SyncEngineContext> context_;
  base::WeakPtr<drive_backend::SyncEngine> sync_engine_;

  base::WeakPtrFactory<SyncWorker> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SyncWorker);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_H_

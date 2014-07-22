// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_DRIVE_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_DRIVE_FILE_SYNC_SERVICE_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/drive/drive_notification_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync_file_system/conflict_resolution_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/api_util_interface.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/local_sync_operation_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/origin_operation_queue.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/remote_change_handler.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace google_apis {
class ResourceList;
}

namespace tracked_objects {
class Location;
}

namespace sync_file_system {

namespace drive_backend {
class LocalSyncDelegate;
class RemoteSyncDelegate;
class SyncTaskManager;
}

// Maintains remote file changes.
// Owned by SyncFileSystemService (which is a per-profile object).
class DriveFileSyncService : public RemoteFileSyncService,
                             public LocalChangeProcessor,
                             public drive_backend::APIUtilObserver,
                             public drive_backend::SyncTaskManager::Client,
                             public base::NonThreadSafe,
                             public base::SupportsWeakPtr<DriveFileSyncService>,
                             public drive::DriveNotificationObserver {
 public:
  typedef base::Callback<void(const SyncStatusCallback& callback)> Task;

  static ConflictResolutionPolicy kDefaultPolicy;

  virtual ~DriveFileSyncService();

  // Creates DriveFileSyncService.
  static scoped_ptr<DriveFileSyncService> Create(Profile* profile);
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  // Creates DriveFileSyncService instance for testing.
  // |metadata_store| must be initialized beforehand.
  static scoped_ptr<DriveFileSyncService> CreateForTesting(
      Profile* profile,
      const base::FilePath& base_dir,
      scoped_ptr<drive_backend::APIUtilInterface> api_util,
      scoped_ptr<DriveMetadataStore> metadata_store);

  // RemoteFileSyncService overrides.
  virtual void AddServiceObserver(Observer* observer) OVERRIDE;
  virtual void AddFileStatusObserver(FileStatusObserver* observer) OVERRIDE;
  virtual void RegisterOrigin(const GURL& origin,
                              const SyncStatusCallback& callback) OVERRIDE;
  virtual void EnableOrigin(const GURL& origin,
                            const SyncStatusCallback& callback) OVERRIDE;
  virtual void DisableOrigin(const GURL& origin,
                             const SyncStatusCallback& callback) OVERRIDE;
  virtual void UninstallOrigin(const GURL& origin,
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
      const FileChange& change,
      const base::FilePath& local_file_path,
      const SyncFileMetadata& local_file_metadata,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback) OVERRIDE;

  // DriveFileSyncClientObserver overrides.
  virtual void OnAuthenticated() OVERRIDE;
  virtual void OnNetworkConnected() OVERRIDE;

  // drive::DriveNotificationObserver implementation.
  virtual void OnNotificationReceived() OVERRIDE;
  virtual void OnPushNotificationEnabled(bool enabled) OVERRIDE;

  // SyncTaskManager::Client overrides.
  virtual void MaybeScheduleNextTask() OVERRIDE;
  virtual void NotifyLastOperationStatus(
      SyncStatusCode sync_status,
      bool used_network) OVERRIDE;
  virtual void RecordTaskLog(scoped_ptr<TaskLogger::TaskLog> log) OVERRIDE;

  static std::string PathToTitle(const base::FilePath& path);
  static base::FilePath TitleToPath(const std::string& title);
  static DriveMetadata::ResourceType SyncFileTypeToDriveMetadataResourceType(
      SyncFileType file_type);
  static SyncFileType DriveMetadataResourceTypeToSyncFileType(
      DriveMetadata::ResourceType resource_type);

 private:
  friend class SyncTaskManager;
  friend class drive_backend::LocalSyncDelegate;
  friend class drive_backend::RemoteSyncDelegate;

  friend class DriveFileSyncServiceFakeTest;
  friend class DriveFileSyncServiceSyncTest;
  friend class DriveFileSyncServiceTest;
  struct ApplyLocalChangeParam;
  struct ProcessRemoteChangeParam;

  typedef base::Callback<
      void(SyncStatusCode status,
           const std::string& resource_id)> ResourceIdCallback;

  explicit DriveFileSyncService(Profile* profile);

  void Initialize(scoped_ptr<drive_backend::SyncTaskManager> task_manager,
                  const SyncStatusCallback& callback);
  void InitializeForTesting(
      scoped_ptr<drive_backend::SyncTaskManager> task_manager,
      const base::FilePath& base_dir,
      scoped_ptr<drive_backend::APIUtilInterface> sync_client,
      scoped_ptr<DriveMetadataStore> metadata_store,
      const SyncStatusCallback& callback);

  void DidInitializeMetadataStore(const SyncStatusCallback& callback,
                                  SyncStatusCode status,
                                  bool created);

  void UpdateServiceStateFromLastOperationStatus(
      SyncStatusCode sync_status,
      google_apis::GDataErrorCode gdata_error);

  // Updates the service state. Also this may notify observers if the
  // service state has been changed from the original value.
  void UpdateServiceState(RemoteServiceState state,
                          const std::string& description);

  void DoRegisterOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback);
  void DoEnableOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback);
  void DoDisableOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback);
  void DoUninstallOrigin(
      const GURL& origin,
      UninstallFlag flag,
      const SyncStatusCallback& callback);
  void DoProcessRemoteChange(
      const SyncFileCallback& sync_callback,
      const SyncStatusCallback& completion_callback);
  void DoApplyLocalChange(
      const FileChange& change,
      const base::FilePath& local_file_path,
      const SyncFileMetadata& local_file_metadata,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback);

  void UpdateRegisteredOrigins();

  void StartBatchSync(const SyncStatusCallback& callback);
  void DidGetDriveDirectoryForOrigin(const GURL& origin,
                                     const SyncStatusCallback& callback,
                                     SyncStatusCode status,
                                     const std::string& resource_id);
  void DidUninstallOrigin(const GURL& origin,
                          const SyncStatusCallback& callback,
                          google_apis::GDataErrorCode error);
  void DidGetLargestChangeStampForBatchSync(
      const SyncStatusCallback& callback,
      const GURL& origin,
      const std::string& resource_id,
      google_apis::GDataErrorCode error,
      int64 largest_changestamp);
  void DidGetDirectoryContentForBatchSync(
      const SyncStatusCallback& callback,
      const GURL& origin,
      const std::string& resource_id,
      int64 largest_changestamp,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> feed);

  void DidProcessRemoteChange(const SyncFileCallback& sync_callback,
                              const SyncStatusCallback& completion_callback,
                              SyncStatusCode status);
  void DidApplyLocalChange(const SyncStatusCallback& callback,
                           SyncStatusCode status);

  // Returns true if |pending_changes_| was updated.
  bool AppendRemoteChange(
      const GURL& origin,
      const google_apis::ResourceEntry& entry,
      int64 changestamp);
  bool AppendFetchChange(
      const GURL& origin,
      const base::FilePath& path,
      const std::string& resource_id,
      SyncFileType file_type);
  bool AppendRemoteChangeInternal(
      const GURL& origin,
      const base::FilePath& path,
      bool is_deleted,
      const std::string& resource_id,
      int64 changestamp,
      const std::string& remote_file_md5,
      const base::Time& updated_time,
      SyncFileType file_type);
  void RemoveRemoteChange(const fileapi::FileSystemURL& url);

  // TODO(kinuko,tzik): Move this out of DriveFileSyncService.
  void MarkConflict(
      const fileapi::FileSystemURL& url,
      DriveMetadata* drive_metadata,
      const SyncStatusCallback& callback);
  void NotifyConflict(
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback,
      SyncStatusCode status);

  // A wrapper implementation to GDataErrorCodeToSyncStatusCode which returns
  // authentication error if the user is not signed in.
  SyncStatusCode GDataErrorCodeToSyncStatusCodeWrapper(
      google_apis::GDataErrorCode error);

  base::FilePath temporary_file_dir_;

  // May start batch sync or incremental sync.
  // This posts either one of following tasks:
  // - StartBatchSyncForOrigin() if it has any pending batch sync origins, or
  // - FetchChangesForIncrementalSync() otherwise.
  //
  // These two methods are called only from this method.
  void MaybeStartFetchChanges();

  void FetchChangesForIncrementalSync(const SyncStatusCallback& callback);
  void DidFetchChangesForIncrementalSync(
      const SyncStatusCallback& callback,
      bool has_new_changes,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> changes);
  bool GetOriginForEntry(const google_apis::ResourceEntry& entry, GURL* origin);
  void NotifyObserversFileStatusChanged(const fileapi::FileSystemURL& url,
                                        SyncFileStatus sync_status,
                                        SyncAction action_taken,
                                        SyncDirection direction);

  void EnsureSyncRootDirectory(const ResourceIdCallback& callback);
  void DidEnsureSyncRoot(const ResourceIdCallback& callback,
                         google_apis::GDataErrorCode error,
                         const std::string& sync_root_resource_id);
  void EnsureOriginRootDirectory(const GURL& origin,
                                 const ResourceIdCallback& callback);
  void DidEnsureSyncRootForOriginRoot(const GURL& origin,
                                      const ResourceIdCallback& callback,
                                      SyncStatusCode status,
                                      const std::string& sync_root_resource_id);
  void DidEnsureOriginRoot(const GURL& origin,
                           const ResourceIdCallback& callback,
                           google_apis::GDataErrorCode error,
                           const std::string& resource_id);

  // This function returns Resouce ID for the sync root directory if available.
  // Returns an empty string 1) when the resource ID has not been initialized
  // yet, and 2) after the service has detected the remote sync root folder was
  // removed.
  std::string sync_root_resource_id();

  scoped_ptr<DriveMetadataStore> metadata_store_;
  scoped_ptr<drive_backend::APIUtilInterface> api_util_;

  Profile* profile_;

  scoped_ptr<drive_backend::SyncTaskManager> task_manager_;

  scoped_ptr<drive_backend::LocalSyncDelegate> running_local_sync_task_;
  scoped_ptr<drive_backend::RemoteSyncDelegate> running_remote_sync_task_;

  // The current remote service state. This does NOT reflect the
  // sync_enabled_ flag, while GetCurrentState() DOES reflect the flag
  // value (i.e. it returns REMOTE_SERVICE_DISABLED when sync_enabled_
  // is false even if state_ is REMOTE_SERVICE_OK).
  RemoteServiceState state_;

  // Indicates if sync is enabled or not. This flag can be turned on or
  // off by SetSyncEnabled() method.  To start synchronization
  // this needs to be true and state_ needs to be REMOTE_SERVICE_OK.
  bool sync_enabled_;

  int64 largest_fetched_changestamp_;

  std::map<GURL, std::string> pending_batch_sync_origins_;

  // Is set to true when there's a fair possibility that we have some
  // remote changes that haven't been fetched yet.
  //
  // This flag is set when:
  // - This gets invalidation notification,
  // - The service is authenticated or becomes online, and
  // - The polling timer is fired.
  //
  // This flag is cleared when:
  // - A batch or incremental sync has been started, and
  // - When all pending batch sync tasks have been finished.
  bool may_have_unfetched_changes_;

  ObserverList<Observer> service_observers_;
  ObserverList<FileStatusObserver> file_status_observers_;

  RemoteChangeHandler remote_change_handler_;
  RemoteChangeProcessor* remote_change_processor_;

  google_apis::GDataErrorCode last_gdata_error_;

  ConflictResolutionResolver conflict_resolution_resolver_;

  OriginOperationQueue pending_origin_operations_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_DRIVE_FILE_SYNC_SERVICE_H_

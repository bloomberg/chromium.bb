// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_SERVICE_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client_interface.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "sync/notifier/invalidation_handler.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_action.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_direction.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

class ExtensionService;

namespace google_apis {
class ResourceList;
}

namespace tracked_objects {
class Location;
}

namespace sync_file_system {

// Maintains remote file changes.
// Owned by SyncFileSystemService (which is a per-profile object).
class DriveFileSyncService
    : public RemoteFileSyncService,
      public LocalChangeProcessor,
      public DriveFileSyncClientObserver,
      public base::NonThreadSafe,
      public syncer::InvalidationHandler {
 public:
  static const char kServiceName[];

  explicit DriveFileSyncService(Profile* profile);
  virtual ~DriveFileSyncService();

  // Creates DriveFileSyncClient instance for testing.
  // |metadata_store| must be initialized beforehand.
  static scoped_ptr<DriveFileSyncService> CreateForTesting(
      Profile* profile,
      const base::FilePath& base_dir,
      scoped_ptr<DriveFileSyncClientInterface> sync_client,
      scoped_ptr<DriveMetadataStore> metadata_store);

  // Destroys |sync_service| and passes the ownership of |sync_client| to caller
  // for testing.
  static scoped_ptr<DriveFileSyncClientInterface>
  DestroyAndPassSyncClientForTesting(
      scoped_ptr<DriveFileSyncService> sync_service);

  // RemoteFileSyncService overrides.
  virtual void AddServiceObserver(Observer* observer) OVERRIDE;
  virtual void AddFileStatusObserver(FileStatusObserver* observer) OVERRIDE;
  virtual void RegisterOriginForTrackingChanges(
      const GURL& origin,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void UnregisterOriginForTrackingChanges(
      const GURL& origin,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void DeleteOriginDirectory(
      const GURL& origin,
      const SyncStatusCallback& callback) OVERRIDE;
  virtual void ProcessRemoteChange(
      RemoteChangeProcessor* processor,
      const SyncFileCallback& callback) OVERRIDE;
  virtual LocalChangeProcessor* GetLocalChangeProcessor() OVERRIDE;
  virtual bool IsConflicting(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void GetRemoteFileMetadata(
      const fileapi::FileSystemURL& url,
      const SyncFileMetadataCallback& callback) OVERRIDE;
  virtual RemoteServiceState GetCurrentState() const OVERRIDE;
  virtual const char* GetServiceName() const OVERRIDE;
  virtual void SetSyncEnabled(bool enabled) OVERRIDE;

  // LocalChangeProcessor overrides.
  virtual void ApplyLocalChange(
      const FileChange& change,
      const base::FilePath& local_file_path,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback) OVERRIDE;

  // DriveFileSyncClientObserver overrides.
  virtual void OnAuthenticated() OVERRIDE;
  virtual void OnNetworkConnected() OVERRIDE;

  // syncer::InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

 private:
  friend class DriveFileSyncServiceTest;
  friend class DriveFileSyncServiceSyncTest;
  class TaskToken;
  struct ProcessRemoteChangeParam;

  enum RemoteSyncType {
    // Smaller number indicates higher priority in ChangeQueue.
    REMOTE_SYNC_TYPE_FETCH = 0,
    REMOTE_SYNC_TYPE_INCREMENTAL = 1,
    REMOTE_SYNC_TYPE_BATCH = 2,
  };

  struct ChangeQueueItem {
    int64 changestamp;
    RemoteSyncType sync_type;
    fileapi::FileSystemURL url;

    ChangeQueueItem();
    ChangeQueueItem(int64 changestamp,
                    RemoteSyncType sync_type,
                    const fileapi::FileSystemURL& url);
  };

  struct ChangeQueueComparator {
    bool operator()(const ChangeQueueItem& left, const ChangeQueueItem& right);
  };

  typedef std::set<ChangeQueueItem, ChangeQueueComparator> PendingChangeQueue;

  struct RemoteChange {
    int64 changestamp;
    std::string resource_id;
    std::string md5_checksum;
    RemoteSyncType sync_type;
    fileapi::FileSystemURL url;
    FileChange change;
    PendingChangeQueue::iterator position_in_queue;

    RemoteChange();
    RemoteChange(int64 changestamp,
                 const std::string& resource_id,
                 const std::string& md5_checksum,
                 RemoteSyncType sync_type,
                 const fileapi::FileSystemURL& url,
                 const FileChange& change,
                 PendingChangeQueue::iterator position_in_queue);
    ~RemoteChange();
  };

  struct RemoteChangeComparator {
    bool operator()(const RemoteChange& left, const RemoteChange& right);
  };

  // TODO(tzik): Consider using std::pair<base::FilePath, FileType> as the key
  // below to support directories and custom conflict handling.
  typedef std::map<base::FilePath, RemoteChange> PathToChangeMap;
  typedef std::map<GURL, PathToChangeMap> OriginToChangesMap;

  // Task types; used for task token handling.
  enum TaskType {
    // No task is holding this token.
    TASK_TYPE_NONE,

    // Token is granted for drive-related async task.
    TASK_TYPE_DRIVE,

    // Token is granted for async database task.
    TASK_TYPE_DATABASE,
  };

  enum LocalSyncOperationType {
    LOCAL_SYNC_OPERATION_ADD,
    LOCAL_SYNC_OPERATION_UPDATE,
    LOCAL_SYNC_OPERATION_DELETE,
    LOCAL_SYNC_OPERATION_NONE,
    LOCAL_SYNC_OPERATION_CONFLICT,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_FAIL,
  };

  DriveFileSyncService(Profile* profile,
                       const base::FilePath& base_dir,
                       scoped_ptr<DriveFileSyncClientInterface> sync_client,
                       scoped_ptr<DriveMetadataStore> metadata_store);

  // This should be called when an async task needs to get a task token.
  // |task_description| is optional but should give human-readable
  // messages that describe the task that is acquiring the token.
  scoped_ptr<TaskToken> GetToken(const tracked_objects::Location& from_here,
                                 TaskType task_type,
                                 const std::string& task_description);
  void NotifyTaskDone(SyncStatusCode status,
                      scoped_ptr<TaskToken> token);
  void UpdateServiceState();
  base::WeakPtr<DriveFileSyncService> AsWeakPtr();

  void DidGetRemoteFileMetadata(
      const SyncFileMetadataCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry);

  // Local synchronization related methods.
  LocalSyncOperationType ResolveLocalSyncOperationType(
      const FileChange& local_file_change,
      const fileapi::FileSystemURL& url);
  void DidApplyLocalChange(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const google_apis::GDataErrorCode error,
      const SyncStatusCallback& callback,
      SyncStatusCode status);
  void DidResolveConflictToRemoteChange(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const std::string& resource_id,
      const SyncStatusCallback& callback,
      SyncStatusCode status);
  void FinalizeLocalSync(
      scoped_ptr<TaskToken> token,
      const SyncStatusCallback& callback,
      SyncStatusCode status);
  void DidUploadNewFileForLocalSync(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      const std::string& resource_id,
      const std::string& file_md5);
  void DidUploadExistingFileForLocalSync(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      const std::string& resource_id,
      const std::string& file_md5);
  void DidDeleteFileForLocalSync(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error);

  void DidInitializeMetadataStore(scoped_ptr<TaskToken> token,
                                  SyncStatusCode status,
                                  bool created);
  void UnregisterInactiveExtensionsIds();

  void GetSyncRootDirectory(scoped_ptr<TaskToken> token,
                            const SyncStatusCallback& callback);
  void DidGetSyncRootDirectory(scoped_ptr<TaskToken> token,
                               const SyncStatusCallback& callback,
                               google_apis::GDataErrorCode error,
                               const std::string& resource_id);
  void DidGetSyncRootForRegisterOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback,
      SyncStatusCode status);
  void StartBatchSyncForOrigin(const GURL& origin,
                               const std::string& resource_id);
  void DidGetDirectoryForOrigin(scoped_ptr<TaskToken> token,
                                const GURL& origin,
                                const SyncStatusCallback& callback,
                                google_apis::GDataErrorCode error,
                                const std::string& resource_id);
  void DidDeleteOriginDirectory(scoped_ptr<TaskToken> token,
                                const SyncStatusCallback& callback,
                                google_apis::GDataErrorCode error);
  void DidGetLargestChangeStampForBatchSync(scoped_ptr<TaskToken> token,
                                            const GURL& origin,
                                            const std::string& resource_id,
                                            google_apis::GDataErrorCode error,
                                            int64 largest_changestamp);
  void DidGetDirectoryContentForBatchSync(
      scoped_ptr<TaskToken> token,
      const GURL& origin,
      int64 largest_changestamp,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> feed);
  void DidRemoveOriginOnMetadataStore(
      scoped_ptr<TaskToken> token,
      const SyncStatusCallback& callback,
      SyncStatusCode status);

  // Remote synchronization related methods.
  void DidPrepareForProcessRemoteChange(
      scoped_ptr<ProcessRemoteChangeParam> param,
      SyncStatusCode status,
      const SyncFileMetadata& metadata,
      const FileChangeList& changes);
  void DidResolveConflictToLocalChange(
      scoped_ptr<ProcessRemoteChangeParam> param,
      SyncStatusCode status);
  void DownloadForRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param);
  void DidGetTemporaryFileForDownload(
      scoped_ptr<ProcessRemoteChangeParam> param,
      bool success);
  void DidDownloadFileForRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      google_apis::GDataErrorCode error,
      const std::string& md5_checksum);
  void DidApplyRemoteChange(
      scoped_ptr<ProcessRemoteChangeParam> param,
      SyncStatusCode status);
  void DidCleanUpForRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      bool success);
  void DeleteMetadataForRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param);
  void CompleteRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      SyncStatusCode status);
  void AbortRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      SyncStatusCode status);
  void FinalizeRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      SyncStatusCode status);

  // Returns true if |pending_changes_| was updated.
  bool AppendRemoteChange(const GURL& origin,
                          const google_apis::ResourceEntry& entry,
                          int64 changestamp,
                          RemoteSyncType sync_type);
  bool AppendFetchChange(const GURL& origin,
                         const base::FilePath& path,
                         const std::string& resource_id);
  bool AppendRemoteChangeInternal(const GURL& origin,
                                  const base::FilePath& path,
                                  bool is_deleted,
                                  const std::string& resource_id,
                                  int64 changestamp,
                                  const std::string& remote_file_md5,
                                  RemoteSyncType sync_type);
  void RemoveRemoteChange(const fileapi::FileSystemURL& url);
  void MaybeMarkAsIncrementalSyncOrigin(const GURL& origin);

  // This returns false if no change is found for the |url|.
  bool GetPendingChangeForFileSystemURL(const fileapi::FileSystemURL& url,
                                        RemoteChange* change) const;

  // A wrapper implementation to GDataErrorCodeToSyncStatusCode which returns
  // authentication error if the user is not signed in.
  SyncStatusCode GDataErrorCodeToSyncStatusCodeWrapper(
      google_apis::GDataErrorCode error) const;

  base::FilePath temporary_file_dir_;

  // May start batch sync or incremental sync.
  // This immediately returns if:
  // - Another task is running (i.e. task_ is null), or
  // - The service state is DISABLED.
  //
  // This calls:
  // - StartBatchSyncForOrigin() if it has any pending batch sync origins, or
  // - FetchChangesForIncrementalSync() otherwise.
  //
  // These two methods are called only from this method.
  void MaybeStartFetchChanges();

  void FetchChangesForIncrementalSync();
  void DidFetchChangesForIncrementalSync(
      scoped_ptr<TaskToken> token,
      bool has_new_changes,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> changes);
  bool GetOriginForEntry(const google_apis::ResourceEntry& entry, GURL* origin);
  void SchedulePolling();
  void OnPollingTimerFired();
  void UpdatePollingDelay(int64 new_delay_sec);
  void RegisterDriveNotifications();
  bool IsDriveNotificationSupported();
  void SetPushNotificationEnabled(syncer::InvalidatorState state);
  void NotifyObserversFileStatusChanged(const fileapi::FileSystemURL& url,
                                        SyncFileStatus sync_status,
                                        SyncAction action_taken,
                                        SyncDirection direction);

  scoped_ptr<DriveMetadataStore> metadata_store_;
  scoped_ptr<DriveFileSyncClientInterface> sync_client_;

  Profile* profile_;
  SyncStatusCode last_operation_status_;
  std::deque<base::Closure> pending_tasks_;

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

  PendingChangeQueue pending_changes_;
  OriginToChangesMap origin_to_changes_map_;

  std::set<GURL> pending_batch_sync_origins_;

  // Absence of |token_| implies a task is running. Incoming tasks should
  // wait for the task to finish in |pending_tasks_| if |token_| is null.
  // Each task must take TaskToken instance from |token_| and must hold it
  // until it finished. And the task must return the instance through
  // NotifyTaskDone when the task finished.
  scoped_ptr<TaskToken> token_;

  // True when Drive File Sync Service is registered for Drive notifications.
  bool push_notification_registered_;
  // True once the first drive notification is received with OK state.
  bool push_notification_enabled_;
  // Timer to trigger fetching changes for incremental sync.
  base::OneShotTimer<DriveFileSyncService> polling_timer_;
  // If polling_delay_seconds_ is negative (<0) the timer won't start.
  int64 polling_delay_seconds_;

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

  // Use WeakPtrFactory instead of SupportsWeakPtr to revoke the weak pointer
  // in |token_|.
  base::WeakPtrFactory<DriveFileSyncService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_SERVICE_H_

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
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"

namespace google_apis {
class ResourceList;
}

namespace tracked_objects {
class Location;
}

namespace sync_file_system {

class DriveMetadataStore;

// Maintains remote file changes.
// Owned by SyncFileSystemService (which is a per-profile object).
class DriveFileSyncService
    : public RemoteFileSyncService,
      public LocalChangeProcessor,
      public DriveFileSyncClientObserver,
      public base::NonThreadSafe {
 public:
  static const char kServiceName[];

  explicit DriveFileSyncService(Profile* profile);
  virtual ~DriveFileSyncService();

  // Creates DriveFileSyncClient instance for testing.
  // |metadata_store| must be initialized beforehand.
  static scoped_ptr<DriveFileSyncService> CreateForTesting(
      const FilePath& base_dir,
      scoped_ptr<DriveFileSyncClient> sync_client,
      scoped_ptr<DriveMetadataStore> metadata_store);

  // RemoteFileSyncService overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void RegisterOriginForTrackingChanges(
      const GURL& origin,
      const fileapi::SyncStatusCallback& callback) OVERRIDE;
  virtual void UnregisterOriginForTrackingChanges(
      const GURL& origin,
      const fileapi::SyncStatusCallback& callback) OVERRIDE;
  virtual void ProcessRemoteChange(
      RemoteChangeProcessor* processor,
      const fileapi::SyncOperationCallback& callback) OVERRIDE;
  virtual LocalChangeProcessor* GetLocalChangeProcessor() OVERRIDE;
  virtual bool IsConflicting(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual void GetConflictFiles(
      const GURL& origin,
      const fileapi::SyncFileSetCallback& callback) OVERRIDE;
  virtual void GetRemoteFileMetadata(
      const fileapi::FileSystemURL& url,
      const fileapi::SyncFileMetadataCallback& callback) OVERRIDE;
  virtual RemoteServiceState GetCurrentState() const OVERRIDE;
  virtual const char* GetServiceName() const OVERRIDE;

  // LocalChangeProcessor overrides.
  virtual void ApplyLocalChange(
      const fileapi::FileChange& change,
      const FilePath& local_file_path,
      const fileapi::FileSystemURL& url,
      const fileapi::SyncStatusCallback& callback) OVERRIDE;

  // DriveFileSyncClientObserver overrides.
  virtual void OnAuthenticated() OVERRIDE;
  virtual void OnNetworkConnected() OVERRIDE;

 private:
  friend class DriveFileSyncServiceTest;
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
    RemoteSyncType sync_type;
    fileapi::FileSystemURL url;
    fileapi::FileChange change;
    PendingChangeQueue::iterator position_in_queue;

    RemoteChange();
    RemoteChange(int64 changestamp,
                 const std::string& resource_id,
                 RemoteSyncType sync_type,
                 const fileapi::FileSystemURL& url,
                 const fileapi::FileChange& change,
                 PendingChangeQueue::iterator position_in_queue);
    ~RemoteChange();
  };

  struct RemoteChangeComparator {
    bool operator()(const RemoteChange& left, const RemoteChange& right);
  };

  // TODO(tzik): Consider using std::pair<FilePath, FileType> as the key below
  // to support directories and custom conflict handling.
  typedef std::map<FilePath, RemoteChange> PathToChangeMap;
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

  DriveFileSyncService(const FilePath& base_dir,
                       scoped_ptr<DriveFileSyncClient> sync_client,
                       scoped_ptr<DriveMetadataStore> metadata_store);

  // This should be called when an async task needs to get a task token.
  // |task_description| is optional but should give human-readable
  // messages that describe the task that is acquiring the token.
  scoped_ptr<TaskToken> GetToken(const tracked_objects::Location& from_here,
                                 TaskType task_type,
                                 const std::string& task_description);
  void NotifyTaskDone(fileapi::SyncStatusCode status,
                      scoped_ptr<TaskToken> token);
  void UpdateServiceState();
  base::WeakPtr<DriveFileSyncService> AsWeakPtr();

  void DidGetRemoteFileMetadata(
      const fileapi::SyncFileMetadataCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry);

  // Local synchronization related methods.
  LocalSyncOperationType ResolveLocalSyncOperationType(
      const fileapi::FileChange& local_file_change,
      const fileapi::FileSystemURL& url);
  void DidApplyLocalChange(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const google_apis::GDataErrorCode error,
      const fileapi::SyncStatusCallback& callback,
      fileapi::SyncStatusCode status);
  void DidResolveConflictToRemoteChange(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const std::string& resource_id,
      const fileapi::SyncStatusCallback& callback,
      fileapi::SyncStatusCode status);
  void FinalizeLocalSync(
      scoped_ptr<TaskToken> token,
      const fileapi::SyncStatusCallback& callback,
      fileapi::SyncStatusCode status);
  void DidUploadNewFileForLocalSync(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const fileapi::SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      const std::string& resource_id,
      const std::string& file_md5);
  void DidUploadExistingFileForLocalSync(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const fileapi::SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      const std::string& resource_id,
      const std::string& file_md5);
  void DidDeleteFileForLocalSync(
      scoped_ptr<TaskToken> token,
      const fileapi::FileSystemURL& url,
      const fileapi::SyncStatusCallback& callback,
      google_apis::GDataErrorCode error);

  void DidInitializeMetadataStore(scoped_ptr<TaskToken> token,
                                  fileapi::SyncStatusCode status,
                                  bool created);
  void GetSyncRootDirectory(scoped_ptr<TaskToken> token,
                            const fileapi::SyncStatusCallback& callback);
  void DidGetSyncRootDirectory(scoped_ptr<TaskToken> token,
                               const fileapi::SyncStatusCallback& callback,
                               google_apis::GDataErrorCode error,
                               const std::string& resource_id);
  void DidGetSyncRootForRegisterOrigin(
      const GURL& origin,
      const fileapi::SyncStatusCallback& callback,
      fileapi::SyncStatusCode status);
  void StartBatchSyncForOrigin(const GURL& origin,
                               const std::string& resource_id);
  void DidGetDirectoryForOrigin(scoped_ptr<TaskToken> token,
                                const GURL& origin,
                                const fileapi::SyncStatusCallback& callback,
                                google_apis::GDataErrorCode error,
                                const std::string& resource_id);
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
      const fileapi::SyncStatusCallback& callback,
      fileapi::SyncStatusCode status);

  // Remote synchronization related methods.
  void DidPrepareForProcessRemoteChange(
      scoped_ptr<ProcessRemoteChangeParam> param,
      fileapi::SyncStatusCode status,
      const fileapi::SyncFileMetadata& metadata,
      const fileapi::FileChangeList& changes);
  void DidResolveConflictToLocalChange(
      scoped_ptr<ProcessRemoteChangeParam> param,
      fileapi::SyncStatusCode status);
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
      fileapi::SyncStatusCode status);
  void DidCleanUpForRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      bool success);
  void DeleteMetadataForRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param);
  void CompleteRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      fileapi::SyncStatusCode status);
  void AbortRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      fileapi::SyncStatusCode status);
  void FinalizeRemoteSync(
      scoped_ptr<ProcessRemoteChangeParam> param,
      fileapi::SyncStatusCode status);

  // Returns true if |pending_changes_| was updated.
  bool AppendRemoteChange(const GURL& origin,
                          const google_apis::ResourceEntry& entry,
                          int64 changestamp,
                          RemoteSyncType sync_type);
  bool AppendFetchChange(const GURL& origin,
                         const FilePath& path,
                         const std::string& resource_id);
  bool AppendRemoteChangeInternal(const GURL& origin,
                                  const FilePath& path,
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
  fileapi::SyncStatusCode GDataErrorCodeToSyncStatusCodeWrapper(
      google_apis::GDataErrorCode error) const;

  FilePath temporary_file_dir_;

  void FetchChangesForIncrementalSync();
  void DidFetchChangesForIncrementalSync(
      scoped_ptr<TaskToken> token,
      bool has_new_changes,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> changes);
  bool GetOriginForEntry(const google_apis::ResourceEntry& entry, GURL* origin);
  void SchedulePolling();

  scoped_ptr<DriveMetadataStore> metadata_store_;
  scoped_ptr<DriveFileSyncClient> sync_client_;

  fileapi::SyncStatusCode last_operation_status_;
  RemoteServiceState state_;
  std::deque<base::Closure> pending_tasks_;

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

  base::OneShotTimer<DriveFileSyncService> polling_timer_;
  int64 polling_delay_seconds_;
  bool polling_enabled_;

  ObserverList<Observer> observers_;

  // Use WeakPtrFactory instead of SupportsWeakPtr to revoke the weak pointer
  // in |token_|.
  base::WeakPtrFactory<DriveFileSyncService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_SERVICE_H_

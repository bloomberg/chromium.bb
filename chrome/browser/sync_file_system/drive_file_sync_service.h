// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_SERVICE_H_

#include <deque>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"

namespace google_apis {
class DocumentFeed;
}

namespace tracked_objects {
class Location;
}

namespace sync_file_system {

class DriveFileSyncClient;
class DriveMetadataStore;

// Maintains remote file changes.
// Owned by SyncFileSystemService (which is a per-profile object).
class DriveFileSyncService
    : public RemoteFileSyncService,
      public LocalChangeProcessor,
      public base::NonThreadSafe,
      public base::SupportsWeakPtr<DriveFileSyncService> {
 public:
  static const char kServiceName[];

  explicit DriveFileSyncService(Profile* profile);
  virtual ~DriveFileSyncService();

  // Creates DriveFileSyncClient instance for testing.
  // |metadata_store| must be initialized beforehand.
  static scoped_ptr<DriveFileSyncService> CreateForTesting(
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
      const fileapi::SyncFileCallback& callback) OVERRIDE;
  virtual LocalChangeProcessor* GetLocalChangeProcessor() OVERRIDE;
  virtual void GetConflictFiles(
      const GURL& origin,
      const fileapi::SyncFileSetCallback& callback) OVERRIDE;
  virtual void GetRemoteFileMetadata(
      const fileapi::FileSystemURL& url,
      const fileapi::SyncFileMetadataCallback& callback) OVERRIDE;

  // LocalChangeProcessor overrides.
  virtual void ApplyLocalChange(
      const fileapi::FileChange& change,
      const FilePath& local_path,
      const fileapi::FileSystemURL& url,
      const fileapi::SyncStatusCallback& callback) OVERRIDE;

 private:
  friend class DriveFileSyncServiceTest;
  class TaskToken;

  struct RemoteChange {
    int64 changestamp;
    fileapi::FileSystemURL url;
    fileapi::FileChange change;

    RemoteChange();
    RemoteChange(int64 changestamp,
                 const fileapi::FileSystemURL& url,
                 const fileapi::FileChange& change);
    ~RemoteChange();
  };

  struct RemoteChangeComparator {
    bool operator()(const RemoteChange& left, const RemoteChange& right);
  };

  typedef std::priority_queue<RemoteChange, std::vector<RemoteChange>,
                              RemoteChangeComparator> ChangeQueue;
  typedef std::map<FilePath, int64> PathToChangeStamp;
  typedef std::map<GURL, PathToChangeStamp> ChangeStampMap;

  DriveFileSyncService(scoped_ptr<DriveFileSyncClient> sync_client,
                       scoped_ptr<DriveMetadataStore> metadata_store);

  scoped_ptr<TaskToken> GetToken(const tracked_objects::Location& from_here);
  void NotifyTaskDone(fileapi::SyncStatusCode status,
                      scoped_ptr<TaskToken> token);

  void DidInitializeMetadataStore(scoped_ptr<TaskToken> token,
                                  fileapi::SyncStatusCode status,
                                  bool created);
  void DidGetSyncRootDirectory(scoped_ptr<TaskToken> token,
                               google_apis::GDataErrorCode error,
                               const std::string& resource_id);
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
      scoped_ptr<google_apis::DocumentFeed> feed);
  void DidRemoveOriginOnMetadataStore(
      scoped_ptr<TaskToken> token,
      const fileapi::SyncStatusCallback& callback,
      fileapi::SyncStatusCode status);

  void TryResumePendingTasks();
  void AppendNewRemoteChange(const GURL& origin,
                             google_apis::DocumentEntry* entry,
                             int64 changestamp,
                             bool batch_sync);
  void DidGetBatchSyncListData(scoped_ptr<TaskToken> token,
                               const GURL& origin,
                               google_apis::GDataErrorCode error,
                               scoped_ptr<base::Value> data);

  scoped_ptr<DriveMetadataStore> metadata_store_;
  scoped_ptr<DriveFileSyncClient> sync_client_;

  fileapi::SyncStatusCode status_;
  std::deque<base::Closure> pending_tasks_;

  int64 largest_changestamp_;
  ChangeQueue pending_changes_;
  ChangeStampMap changestamp_map_;

  // Absence of |token_| implies a task is running. Incoming tasks should
  // wait for the task to finish in |pending_tasks_| if |token_| is null.
  // Each task must take TaskToken instance from |token_| and must hold it
  // until it finished. And the task must return the instance through
  // NotifyTaskDone when the task finished.
  scoped_ptr<TaskToken> token_;

  ObserverList<Observer> observers_;
  base::WeakPtrFactory<DriveFileSyncService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_SERVICE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYNC_CLIENT_H_

#include <deque>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_cache_observer.h"
#include "chrome/browser/chromeos/drive/drive_file_system_observer.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "net/base/network_change_notifier.h"

class Profile;
class PrefChangeRegistrar;

namespace drive {

class DriveEntryProto;
class DriveFileSystemInterface;
class DrivePrefetcher;

// The DriveSyncClient is used to synchronize pinned files on Drive and the
// cache on the local drive. The sync client works as follows.
//
// When the user pins files on Drive, this client is notified about the files
// that get pinned, and queues tasks and starts fetching these files in the
// background.
//
// When the user unpins files on Drive, this client is notified about the
// files that get unpinned, cancels tasks if these are still in the queue.
//
// If the user logs out before fetching of the pinned files is complete, this
// client resumes fetching operations next time the user logs in, based on
// the states left in the cache.
class DriveSyncClient
    : public DriveFileSystemObserver,
      public DriveCacheObserver {
 public:
  // Types of sync tasks.
  enum SyncType {
    FETCH,  // Fetch a file from the Drive server.
    UPLOAD,  // Upload a file to the Drive server.
  };

  // |profile| is used to access user preferences.
  // |file_system| is used access the
  // cache (ex. store a file to the cache when the file is downloaded).
  DriveSyncClient(Profile* profile,
                  DriveFileSystemInterface* file_system,
                  DriveCache* cache);
  virtual ~DriveSyncClient();

  // Initializes the DriveSyncClient.
  void Initialize();

  // DriveFileSystemInterface::Observer overrides.
  virtual void OnInitialLoadFinished(DriveFileError error) OVERRIDE;
  virtual void OnFeedFromServerLoaded() OVERRIDE;

  // DriveCache::Observer overrides.
  virtual void OnCachePinned(const std::string& resource_id,
                             const std::string& md5) OVERRIDE;
  virtual void OnCacheUnpinned(const std::string& resource_id,
                               const std::string& md5) OVERRIDE;
  virtual void OnCacheCommitted(const std::string& resource_id) OVERRIDE;

  // Starts processing the backlog (i.e. pinned-but-not-filed files and
  // dirty-but-not-uploaded files). Kicks off retrieval of the resource
  // IDs of these files, and then starts the sync loop.
  void StartProcessingBacklog();

  // Starts checking the existing pinned files to see if these are
  // up-to-date. If stale files are detected, the resource IDs of these files
  // are added to the queue and the sync loop is started.
  void StartCheckingExistingPinnedFiles();

  // Returns the resource IDs in |queue_| for the given sync type. Used only
  // for testing.
  std::vector<std::string> GetResourceIdsForTesting(SyncType sync_type) const;

  // Adds the resource ID to the queue. Used only for testing.
  void AddResourceIdForTesting(SyncType sync_type,
                               const std::string& resource_id) {
    AddTaskToQueue(sync_type, resource_id);
  }

  // Sets a delay for testing.
  void set_delay_for_testing(const base::TimeDelta& delay) {
    delay_ = delay;
  }

  // Starts the sync loop if it's not running.
  void StartSyncLoop();

 private:
  friend class DriveSyncClientTest;

  // Adds the given task to the queue. If the same task is queued, remove the
  // existing one, and adds a new one to the end of the queue.
  void AddTaskToQueue(SyncType type, const std::string& resource_id);

  // Called when a task is ready to be added to the queue.
  void StartTask(SyncType type, const std::string& resource_id);

  // Called when the resource IDs of files in the backlog are obtained.
  void OnGetResourceIdsOfBacklog(const std::vector<std::string>* to_fetch,
                                 const std::vector<std::string>* to_upload);

  // Called when the resource ID of a pinned file is obtained.
  void OnGetResourceIdOfExistingPinnedFile(const std::string& resource_id,
                                           const DriveCacheEntry& cache_entry);

  // Called when a file entry is obtained.
  void OnGetEntryInfoByResourceId(const std::string& resource_id,
                                  const DriveCacheEntry& cache_entry,
                                  DriveFileError error,
                                  const base::FilePath& file_path,
                                  scoped_ptr<DriveEntryProto> entry_proto);

  // Called when a cache entry is obtained.
  void OnGetCacheEntry(const std::string& resource_id,
                       const std::string& latest_md5,
                       bool success,
                       const DriveCacheEntry& cache_entry);

  // Called when an existing cache entry and the local files are removed.
  void OnRemove(const std::string& resource_id, DriveFileError error);

  // Called when a file is pinned.
  void OnPinned(const std::string& resource_id, DriveFileError error);

  // Called when the file for |resource_id| is fetched.
  // Calls DoSyncLoop() to go back to the sync loop.
  void OnFetchFileComplete(const std::string& resource_id,
                           DriveFileError error,
                           const base::FilePath& local_path,
                           const std::string& ununsed_mime_type,
                           DriveFileType file_type);

  // Called when the file for |resource_id| is uploaded.
  // Calls DoSyncLoop() to go back to the sync loop.
  void OnUploadFileComplete(const std::string& resource_id,
                            DriveFileError error);

  Profile* profile_;
  DriveFileSystemInterface* file_system_;  // Owned by DriveSystemService.
  DriveCache* cache_;  // Owned by DriveSystemService.

  // List of the resource ids of resources which have a fetch task created.
  std::set<std::string> fetch_list_;

  // List of the resource ids of resources which have a upload task created.
  std::set<std::string> upload_list_;

  // Fetch tasks which have been created, but not started yet.  If they are
  // removed before starting, they will be cancelled.
  std::set<std::string> pending_fetch_list_;

  // The delay is used for delaying processing SyncTasks in DoSyncLoop().
  base::TimeDelta delay_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveSyncClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveSyncClient);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYNC_CLIENT_H_

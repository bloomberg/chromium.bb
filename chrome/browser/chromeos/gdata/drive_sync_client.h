// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SYNC_CLIENT_H_

#include <deque>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "content/public/browser/notification_observer.h"
#include "net/base/network_change_notifier.h"

class Profile;
class PrefChangeRegistrar;

namespace gdata {

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
//
// TODO(satorux): This client should also upload pinned but dirty (locally
// edited) files to Drive. Will work on this once downloading is done.
// crosbug.com/27836.
class DriveSyncClient
    : public DriveFileSystemInterface::Observer,
      public DriveCache::Observer,
      public content::NotificationObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver{
 public:
  // Types of sync tasks.
  enum SyncType {
    FETCH,  // Fetch a file from the Drive server.
    UPLOAD,  // Upload a file to the Drive server.
  };

  // The struct is used to queue tasks for fetching and uploading.
  struct SyncTask {
    SyncTask(SyncType in_sync_type,
             const std::string& in_resource_id,
             const base::Time& in_timestamp);

    SyncType sync_type;
    std::string resource_id;
    base::Time timestamp;
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
  virtual void OnInitialLoadFinished() OVERRIDE;
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
    queue_.push_back(SyncTask(sync_type, resource_id, base::Time::Now()));
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
  void AddTaskToQueue(const SyncTask& sync_task);

  // Runs the sync loop that fetches/uploads files in |queue_|. One file is
  // fetched/uploaded at a time, rather than in parallel. The loop ends when
  // the queue becomes empty.
  void DoSyncLoop();

  // Returns true if we should stop the sync loop.
  bool ShouldStopSyncLoop();

  // Called when the resource IDs of files in the backlog are obtained.
  void OnGetResourceIdsOfBacklog(
      const std::vector<std::string>& to_fetch,
      const std::vector<std::string>& to_upload);

  // Called when the resource IDs of pinned files are obtained.
  void OnGetResourceIdsOfExistingPinnedFiles(
    const std::vector<std::string>& resource_ids);

  // Called when a file entry is obtained.
  void OnGetEntryInfoByResourceId(const std::string& resource_id,
                                 DriveFileError error,
                                 const FilePath& file_path,
                                 scoped_ptr<DriveEntryProto> entry_proto);

  // Called when a cache entry is obtained.
  void OnGetCacheEntry(const std::string& resource_id,
                       const std::string& latest_md5,
                       bool success,
                       const DriveCacheEntry& cache_entry);

  // Called when an existing cache entry and the local files are removed.
  void OnRemove(DriveFileError error,
                const std::string& resource_id,
                const std::string& md5);

  // Called when a file is pinned.
  void OnPinned(DriveFileError error,
                const std::string& resource_id,
                const std::string& md5);

  // Called when the file for |resource_id| is fetched.
  // Calls DoSyncLoop() to go back to the sync loop.
  void OnFetchFileComplete(const SyncTask& sync_task,
                           DriveFileError error,
                           const FilePath& local_path,
                           const std::string& ununsed_mime_type,
                           DriveFileType file_type);

  // Called when the file for |resource_id| is uploaded.
  // Calls DoSyncLoop() to go back to the sync loop.
  void OnUploadFileComplete(const std::string& resource_id,
                            DriveFileError error);

  // content::NotificationObserver override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // net::NetworkChangeNotifier::ConnectionTypeObserver override.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  Profile* profile_;
  DriveFileSystemInterface* file_system_;  // Owned by DriveSystemService.
  DriveCache* cache_;  // Owned by DriveSystemService.
  scoped_ptr<PrefChangeRegistrar> registrar_;

  // The queue of tasks used to fetch/upload files in the background
  // thread. Note that this class does not use a lock to protect |queue_| as
  // all methods touching |queue_| run on the UI thread.
  std::deque<SyncTask> queue_;

  // The delay is used for delaying processing SyncTasks in DoSyncLoop().
  base::TimeDelta delay_;

  // True if the sync loop is running.
  bool sync_loop_is_running_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveSyncClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveSyncClient);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SYNC_CLIENT_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CLIENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/file_errors.h"

namespace base {
class SequencedTaskRunner;
}

namespace drive {

class FileCacheEntry;
class JobScheduler;
class ResourceEntry;

namespace file_system {
class DownloadOperation;
class OperationObserver;
class UpdateOperation;
}

namespace internal {

class FileCache;
class ResourceMetadata;

// The SyncClient is used to synchronize pinned files on Drive and the
// cache on the local drive.
//
// If the user logs out before fetching of the pinned files is complete, this
// client resumes fetching operations next time the user logs in, based on
// the states left in the cache.
class SyncClient {
 public:
  // Types of sync tasks.
  enum SyncType {
    FETCH,  // Fetch a file from the Drive server.
    UPLOAD,  // Upload a file to the Drive server.
    UPLOAD_NO_CONTENT_CHECK,  // Upload a file without checking if the file is
                              // really modified. This type is used for upload
                              // retry tasks that should have already run the
                              // check at least once.
  };

  SyncClient(base::SequencedTaskRunner* blocking_task_runner,
             file_system::OperationObserver* observer,
             JobScheduler* scheduler,
             ResourceMetadata* metadata,
             FileCache* cache,
             const base::FilePath& temporary_file_directory);
  virtual ~SyncClient();

  // Adds a fetch task to the queue.
  void AddFetchTask(const std::string& local_id);

  // Removes a fetch task from the queue.
  void RemoveFetchTask(const std::string& local_id);

  // Adds an upload task to the queue.
  void AddUploadTask(const std::string& local_id);

  // Starts processing the backlog (i.e. pinned-but-not-filed files and
  // dirty-but-not-uploaded files). Kicks off retrieval of the local
  // IDs of these files, and then starts the sync loop.
  void StartProcessingBacklog();

  // Starts checking the existing pinned files to see if these are
  // up-to-date. If stale files are detected, the local IDs of these files
  // are added to the queue and the sync loop is started.
  void StartCheckingExistingPinnedFiles();

  // Sets a delay for testing.
  void set_delay_for_testing(const base::TimeDelta& delay) {
    delay_ = delay;
  }

  // Starts the sync loop if it's not running.
  void StartSyncLoop();

 private:
  // Adds the given task to the queue. If the same task is queued, remove the
  // existing one, and adds a new one to the end of the queue.
  void AddTaskToQueue(SyncType type,
                      const std::string& local_id,
                      const base::TimeDelta& delay);

  // Called when a task is ready to be added to the queue.
  void StartTask(SyncType type, const std::string& local_id);

  // Called when the local IDs of files in the backlog are obtained.
  void OnGetLocalIdsOfBacklog(const std::vector<std::string>* to_fetch,
                                 const std::vector<std::string>* to_upload);

  // Called when the local ID of a pinned file is obtained.
  void OnGetLocalIdOfExistingPinnedFile(const std::string& local_id,
                                        const FileCacheEntry& cache_entry);

  // Called when a file entry is obtained.
  void OnGetResourceEntryById(const std::string& local_id,
                              const FileCacheEntry& cache_entry,
                              FileError error,
                              scoped_ptr<ResourceEntry> entry);

  // Called when a cache entry is obtained.
  void OnGetCacheEntry(const std::string& local_id,
                       const std::string& latest_md5,
                       bool success,
                       const FileCacheEntry& cache_entry);

  // Called when an existing cache entry and the local files are removed.
  void OnRemove(const std::string& local_id, FileError error);

  // Called when a file is pinned.
  void OnPinned(const std::string& local_id, FileError error);

  // Called when the file for |local_id| is fetched.
  // Calls DoSyncLoop() to go back to the sync loop.
  void OnFetchFileComplete(const std::string& local_id,
                           FileError error,
                           const base::FilePath& local_path,
                           scoped_ptr<ResourceEntry> entry);

  // Called when the file for |local_id| is uploaded.
  // Calls DoSyncLoop() to go back to the sync loop.
  void OnUploadFileComplete(const std::string& local_id, FileError error);

  ResourceMetadata* metadata_;
  FileCache* cache_;

  // Used to fetch pinned files.
  scoped_ptr<file_system::DownloadOperation> download_operation_;

  // Used to upload committed files.
  scoped_ptr<file_system::UpdateOperation> update_operation_;

  // List of the local ids of resources which have a fetch task created.
  std::set<std::string> fetch_list_;

  // List of the local ids of resources which have a upload task created.
  std::set<std::string> upload_list_;

  // Fetch tasks which have been created, but not started yet.  If they are
  // removed before starting, they will be cancelled.
  std::set<std::string> pending_fetch_list_;

  // The delay is used for delaying processing tasks in AddTaskToQueue().
  base::TimeDelta delay_;

  // The delay is used for delaying retry of tasks on server errors.
  base::TimeDelta long_delay_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SyncClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncClient);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CLIENT_H_

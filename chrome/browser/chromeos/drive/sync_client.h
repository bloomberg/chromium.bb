// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SYNC_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"

namespace base {
class SequencedTaskRunner;
}

namespace drive {

class FileCacheEntry;
class JobScheduler;
class ResourceEntry;
struct ClientContext;

namespace file_system {
class DownloadOperation;
class OperationObserver;
}

namespace internal {

class ChangeListLoader;
class EntryUpdatePerformer;
class FileCache;
class LoaderController;
class ResourceMetadata;

// The SyncClient is used to synchronize pinned files on Drive and the
// cache on the local drive.
//
// If the user logs out before fetching of the pinned files is complete, this
// client resumes fetching operations next time the user logs in, based on
// the states left in the cache.
class SyncClient {
 public:
  SyncClient(base::SequencedTaskRunner* blocking_task_runner,
             file_system::OperationObserver* observer,
             JobScheduler* scheduler,
             ResourceMetadata* metadata,
             FileCache* cache,
             LoaderController* loader_controller,
             const base::FilePath& temporary_file_directory);
  virtual ~SyncClient();

  // Adds a fetch task.
  void AddFetchTask(const std::string& local_id);

  // Removes a fetch task.
  void RemoveFetchTask(const std::string& local_id);

  // Adds a update task.
  void AddUpdateTask(const ClientContext& context, const std::string& local_id);

  // Starts processing the backlog (i.e. pinned-but-not-filed files and
  // dirty-but-not-uploaded files). Kicks off retrieval of the local
  // IDs of these files, and then starts the sync loop.
  void StartProcessingBacklog();

  // Starts checking the existing pinned files to see if these are
  // up-to-date. If stale files are detected, the local IDs of these files
  // are added and the sync loop is started.
  void StartCheckingExistingPinnedFiles();

  // Sets a delay for testing.
  void set_delay_for_testing(const base::TimeDelta& delay) {
    delay_ = delay;
  }

  // Starts the sync loop if it's not running.
  void StartSyncLoop();

 private:
  // Types of sync tasks.
  enum SyncType {
    FETCH,  // Fetch a file from the Drive server.
    UPDATE,  // Updates an entry's metadata or content on the Drive server.
  };

  // States of sync tasks.
  enum SyncState {
    PENDING,
    RUNNING,
  };

  struct SyncTask {
    SyncTask();
    ~SyncTask();
    SyncState state;
    base::Closure task;
    bool should_run_again;
    base::Closure cancel_closure;
  };

  typedef std::map<std::pair<SyncType, std::string>, SyncTask> SyncTasks;

  // Adds a FETCH task.
  void AddFetchTaskInternal(const std::string& local_id,
                            const base::TimeDelta& delay);

  // Adds a UPDATE task.
  void AddUpdateTaskInternal(const ClientContext& context,
                             const std::string& local_id,
                             const base::TimeDelta& delay);

  // Adds the given task. If the same task is found, does nothing.
  void AddTask(const SyncTasks::key_type& key,
               const SyncTask& task,
               const base::TimeDelta& delay);

  // Called when a task is ready to start.
  void StartTask(const SyncTasks::key_type& key);

  // Called when the local IDs of files in the backlog are obtained.
  void OnGetLocalIdsOfBacklog(const std::vector<std::string>* to_fetch,
                              const std::vector<std::string>* to_update);

  // Adds fetch tasks.
  void AddFetchTasks(const std::vector<std::string>* local_ids);

  // Used as GetFileContentInitializedCallback.
  void OnGetFileContentInitialized(
      FileError error,
      scoped_ptr<ResourceEntry> entry,
      const base::FilePath& local_cache_file_path,
      const base::Closure& cancel_download_closure);

  // Erases the task and returns true if task is completed.
  bool OnTaskComplete(SyncType type, const std::string& local_id);

  // Called when the file for |local_id| is fetched.
  void OnFetchFileComplete(const std::string& local_id,
                           FileError error,
                           const base::FilePath& local_path,
                           scoped_ptr<ResourceEntry> entry);

  // Called when the entry is updated.
  void OnUpdateComplete(const std::string& local_id, FileError error);

  // Adds update tasks for |entries|.
  void AddChildUpdateTasks(const ResourceEntryVector* entries,
                           FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  file_system::OperationObserver* operation_observer_;
  ResourceMetadata* metadata_;
  FileCache* cache_;

  // Used to fetch pinned files.
  scoped_ptr<file_system::DownloadOperation> download_operation_;

  // Used to update entry metadata.
  scoped_ptr<EntryUpdatePerformer> entry_update_performer_;

  // Sync tasks to be processed.
  SyncTasks tasks_;

  // The delay is used for delaying processing tasks in AddTask().
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

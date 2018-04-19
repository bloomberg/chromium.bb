// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_CHANGE_LIST_LOADER_H_
#define COMPONENTS_DRIVE_CHROMEOS_CHANGE_LIST_LOADER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/drive/file_errors.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/drive_common_callbacks.h"

namespace base {
class CancellationFlag;
class ScopedClosureRunner;
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace google_apis {
class AboutResource;
}  // namespace google_apis

namespace drive {

class EventLogger;
class JobScheduler;

namespace internal {

class AboutResourceLoader;
class ChangeList;
class ChangeListLoaderObserver;
class ChangeListProcessor;
class ResourceMetadata;

// Delays execution of tasks as long as more than one lock is alive.
// Used to ensure that ChangeListLoader does not cause race condition by adding
// new entries created by sync tasks before they do.
// All code which may add entries found on the server to the local metadata
// should use this class.
class LoaderController {
 public:
  LoaderController();
  ~LoaderController();

  // Increments the lock count and returns an object which decrements the count
  // on its destruction.
  // While the lock count is positive, tasks will be pending.
  std::unique_ptr<base::ScopedClosureRunner> GetLock();

  // Runs the task if the lock count is 0, otherwise it will be pending.
  void ScheduleRun(const base::Closure& task);

 private:
  // Decrements the lock count.
  void Unlock();

  int lock_count_;
  std::vector<base::Closure> pending_tasks_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<LoaderController> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(LoaderController);
};

// ChangeListLoader is used to load the change list, the full resource list,
// and directory contents, from Google Drive API.  The class also updates the
// resource metadata with the change list loaded from the server.
//
// Note that the difference between "resource list" and "change list" is
// subtle hence the two words are often used interchangeably. To be precise,
// "resource list" refers to metadata from the server when fetching the full
// resource metadata, or fetching directory contents, whereas "change list"
// refers to metadata from the server when fetching changes (delta).
class ChangeListLoader {
 public:
  // Resource feed fetcher from the server.
  class FeedFetcher;

  ChangeListLoader(EventLogger* logger,
                   base::SequencedTaskRunner* blocking_task_runner,
                   ResourceMetadata* resource_metadata,
                   JobScheduler* scheduler,
                   AboutResourceLoader* about_resource_loader,
                   LoaderController* apply_task_controller);
  ~ChangeListLoader();

  // Indicates whether there is a request for full resource list or change
  // list fetching is in flight (i.e. directory contents fetching does not
  // count).
  bool IsRefreshing() const;

  // Adds and removes the observer.
  void AddObserver(ChangeListLoaderObserver* observer);
  void RemoveObserver(ChangeListLoaderObserver* observer);

  // Checks for updates on the server. Does nothing if the change list is now
  // being loaded or refreshed. |callback| must not be null.
  // Note: |callback| will be called if the check for updates actually
  // runs, i.e. it may NOT be called if the checking is ignored.
  void CheckForUpdates(const FileOperationCallback& callback);

  // Starts the change list loading if needed. If the locally stored metadata is
  // available, runs |callback| immediately and starts checking server for
  // updates in background. If the locally stored metadata is not available,
  // starts loading from the server, and runs |callback| to tell the result to
  // the caller when it is finished.
  //
  // |callback| must not be null.
  void LoadIfNeeded(const FileOperationCallback& callback);

 private:
  // Starts the resource metadata loading and calls |callback| when it's done.
  void Load(const FileOperationCallback& callback);
  void LoadAfterGetLargestChangestamp(bool is_initial_load,
                                      const int64_t* local_changestamp,
                                      FileError error);
  void LoadAfterGetAboutResource(
      int64_t local_changestamp,
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::AboutResource> about_resource);

  // Part of Load().
  // This function should be called when the change list load is complete.
  // Flushes the callbacks for change list loading and all directory loading.
  void OnChangeListLoadComplete(FileError error);

  // Called when the loading about_resource_loader_->UpdateAboutResource is
  // completed.
  void OnAboutResourceUpdated(
      google_apis::DriveApiErrorCode error,
      std::unique_ptr<google_apis::AboutResource> resource);

  // ================= Implementation for change list loading =================

  // Part of LoadFromServerIfNeeded().
  // Starts loading the change list since |local_changestamp|, or the full
  // resource list if |start_changestamp| is zero. If there's no changes since
  // then, and there are no new team drives changes to apply from
  // team_drives_change_lists, finishes early.
  // TODO(sashab): Currently, team_drives_change_lists always contains all of
  // the team drives. Update this so team_drives_change_lists is only filled
  // when the TD flag is newly turned on or local data cleared. crbug.com/829154
  void LoadChangeListFromServer(
      std::unique_ptr<google_apis::AboutResource> about_resource,
      int64_t local_changestamp,
      FileError error,
      std::vector<std::unique_ptr<ChangeList>> team_drives_change_lists);

  // Part of LoadChangeListFromServer().
  // Called when the entire change list is loaded.
  void LoadChangeListFromServerAfterLoadChangeList(
      std::unique_ptr<google_apis::AboutResource> about_resource,
      bool is_delta_update,
      std::vector<std::unique_ptr<ChangeList>> team_drives_change_lists,
      FileError error,
      std::vector<std::unique_ptr<ChangeList>> change_lists);

  // Part of LoadChangeListFromServer().
  // Called when the resource metadata is updated.
  void LoadChangeListFromServerAfterUpdate(
      ChangeListProcessor* change_list_processor,
      bool should_notify_changed_directories,
      const base::Time& start_time,
      FileError error);

  EventLogger* logger_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<base::CancellationFlag> in_shutdown_;
  ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  AboutResourceLoader* about_resource_loader_;  // Not owned.
  LoaderController* loader_controller_;  // Not owned.
  base::ObserverList<ChangeListLoaderObserver> observers_;
  std::vector<FileOperationCallback> pending_load_callback_;
  FileOperationCallback pending_update_check_callback_;

  // Running feed fetcher.
  std::unique_ptr<FeedFetcher> change_feed_fetcher_;

  // True if the full resource list is loaded (i.e. the resource metadata is
  // stored locally).
  bool loaded_;

  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChangeListLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChangeListLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_CHANGE_LIST_LOADER_H_

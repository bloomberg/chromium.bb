// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "google_apis/drive/gdata_errorcode.h"

class GURL;

namespace base {
class ScopedClosureRunner;
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace google_apis {
class AboutResource;
class ResourceList;
}  // namespace google_apis

namespace drive {

class EventLogger;
class JobScheduler;
class ResourceEntry;

namespace internal {

class ChangeList;
class ChangeListLoaderObserver;
class ChangeListProcessor;
class DirectoryLoader;
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
  scoped_ptr<base::ScopedClosureRunner> GetLock();

  // Runs the task if the lock count is 0, otherwise it will be pending.
  void ScheduleRun(const base::Closure& task);

 private:
  // Decrements the lock count.
  void Unlock();

  int lock_count_;
  std::vector<base::Closure> pending_tasks_;

  base::WeakPtrFactory<LoaderController> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(LoaderController);
};

// This class is responsible to load AboutResource from the server and cache it.
class AboutResourceLoader {
 public:
  explicit AboutResourceLoader(JobScheduler* scheduler);
  ~AboutResourceLoader();

  // Returns the cached about resource.
  // NULL is returned if the cache is not available.
  const google_apis::AboutResource* cached_about_resource() const {
    return cached_about_resource_.get();
  }

  // Gets the 'latest' about resource and asynchronously runs |callback|. I.e.,
  // 1) If the last call to UpdateAboutResource call is in-flight, wait for it.
  // 2) Otherwise, if the resource is cached, just returns the cached value.
  // 3) If neither of the above hold, queries the API server by calling
  //   |UpdateAboutResource|.
  void GetAboutResource(const google_apis::AboutResourceCallback& callback);

  // Gets the about resource from the server, and caches it if successful. This
  // function calls JobScheduler::GetAboutResource internally. The cache will be
  // used in |GetAboutResource|.
  void UpdateAboutResource(const google_apis::AboutResourceCallback& callback);

 private:
  // Part of UpdateAboutResource().
  // This function should be called when the latest about resource is being
  // fetched from the server. The retrieved about resource is cloned, and one is
  // cached and the other is passed to callbacks associated with |task_id|.
  void UpdateAboutResourceAfterGetAbout(
      int task_id,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  JobScheduler* scheduler_;
  scoped_ptr<google_apis::AboutResource> cached_about_resource_;

  // Identifier to denote the latest UpdateAboutResource call.
  int current_update_task_id_;
  // Mapping from each UpdateAboutResource task ID to the corresponding
  // callbacks. Note that there will be multiple callbacks for a single task
  // when GetAboutResource is called before the task completes.
  std::map<int, std::vector<google_apis::AboutResourceCallback> >
      pending_callbacks_;

  base::WeakPtrFactory<AboutResourceLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AboutResourceLoader);
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
                                      const int64* local_changestamp,
                                      FileError error);
  void LoadAfterGetAboutResource(
      int64 local_changestamp,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of Load().
  // This function should be called when the change list load is complete.
  // Flushes the callbacks for change list loading and all directory loading.
  void OnChangeListLoadComplete(FileError error);

  // ================= Implementation for change list loading =================

  // Part of LoadFromServerIfNeeded().
  // Starts loading the change list since |start_changestamp|, or the full
  // resource list if |start_changestamp| is zero.
  void LoadChangeListFromServer(int64 start_changestamp);

  // Part of LoadChangeListFromServer().
  // Called when the entire change list is loaded.
  void LoadChangeListFromServerAfterLoadChangeList(
      scoped_ptr<google_apis::AboutResource> about_resource,
      bool is_delta_update,
      FileError error,
      ScopedVector<ChangeList> change_lists);

  // Part of LoadChangeListFromServer().
  // Called when the resource metadata is updated.
  void LoadChangeListFromServerAfterUpdate(
      ChangeListProcessor* change_list_processor,
      bool should_notify_changed_directories,
      const base::Time& start_time,
      FileError error);

  EventLogger* logger_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  AboutResourceLoader* about_resource_loader_;  // Not owned.
  LoaderController* loader_controller_;  // Not owned.
  ObserverList<ChangeListLoaderObserver> observers_;
  std::vector<FileOperationCallback> pending_load_callback_;
  FileOperationCallback pending_update_check_callback_;

  // Running feed fetcher.
  scoped_ptr<FeedFetcher> change_feed_fetcher_;

  // True if the full resource list is loaded (i.e. the resource metadata is
  // stored locally).
  bool loaded_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChangeListLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChangeListLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

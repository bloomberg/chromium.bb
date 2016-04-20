// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_DIRECTORY_LOADER_H_
#define COMPONENTS_DRIVE_CHROMEOS_DIRECTORY_LOADER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/file_errors.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/drive_common_callbacks.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class AboutResource;
}  // namespace google_apis

namespace drive {

class EventLogger;
class JobScheduler;
class ResourceEntry;

namespace internal {

class AboutResourceLoader;
class ChangeList;
class ChangeListLoaderObserver;
class DirectoryFetchInfo;
class LoaderController;
class ResourceMetadata;

// DirectoryLoader is used to load directory contents.
class DirectoryLoader {
 public:
  DirectoryLoader(EventLogger* logger,
                  base::SequencedTaskRunner* blocking_task_runner,
                  ResourceMetadata* resource_metadata,
                  JobScheduler* scheduler,
                  AboutResourceLoader* about_resource_loader,
                  LoaderController* apply_task_controller);
  ~DirectoryLoader();

  // Adds and removes the observer.
  void AddObserver(ChangeListLoaderObserver* observer);
  void RemoveObserver(ChangeListLoaderObserver* observer);

  // Reads the directory contents.
  // |entries_callback| can be null.
  // |completion_callback| must not be null.
  void ReadDirectory(const base::FilePath& directory_path,
                     const ReadDirectoryEntriesCallback& entries_callback,
                     const FileOperationCallback& completion_callback);

 private:
  class FeedFetcher;
  struct ReadDirectoryCallbackState;

  // Part of ReadDirectory().
  void ReadDirectoryAfterGetEntry(
      const base::FilePath& directory_path,
      const ReadDirectoryEntriesCallback& entries_callback,
      const FileOperationCallback& completion_callback,
      bool should_try_loading_parent,
      const ResourceEntry* entry,
      FileError error);
  void ReadDirectoryAfterLoadParent(
      const base::FilePath& directory_path,
      const ReadDirectoryEntriesCallback& entries_callback,
      const FileOperationCallback& completion_callback,
      FileError error);
  void ReadDirectoryAfterGetAboutResource(
      const std::string& local_id,
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::AboutResource> about_resource);
  void ReadDirectoryAfterCheckLocalState(
      std::unique_ptr<google_apis::AboutResource> about_resource,
      const std::string& local_id,
      const ResourceEntry* entry,
      const int64_t* local_changestamp,
      FileError error);

  // Part of ReadDirectory().
  // This function should be called when the directory load is complete.
  // Flushes the callbacks waiting for the directory to be loaded.
  void OnDirectoryLoadComplete(const std::string& local_id, FileError error);
  void OnDirectoryLoadCompleteAfterRead(const std::string& local_id,
                                        const ResourceEntryVector* entries,
                                        FileError error);

  // Sends |entries| to the callbacks.
  void SendEntries(const std::string& local_id,
                   const ResourceEntryVector& entries);

  // ================= Implementation for directory loading =================
  // Loads the directory contents from server, and updates the local metadata.
  // Runs |callback| when it is finished.
  void LoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info);

  // Part of LoadDirectoryFromServer() for a normal directory.
  void LoadDirectoryFromServerAfterLoad(
      const DirectoryFetchInfo& directory_fetch_info,
      FeedFetcher* fetcher,
      FileError error);

  // Part of LoadDirectoryFromServer().
  void LoadDirectoryFromServerAfterUpdateChangestamp(
      const DirectoryFetchInfo& directory_fetch_info,
      const base::FilePath* directory_path,
      FileError error);

  EventLogger* logger_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  AboutResourceLoader* about_resource_loader_;  // Not owned.
  LoaderController* loader_controller_;  // Not owned.
  base::ObserverList<ChangeListLoaderObserver> observers_;
  typedef std::map<std::string, std::vector<ReadDirectoryCallbackState> >
      LoadCallbackMap;
  LoadCallbackMap pending_load_callback_;

  // Set of the running feed fetcher for the fast fetch.
  std::set<FeedFetcher*> fast_fetch_feed_fetcher_set_;

  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DirectoryLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DirectoryLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_DIRECTORY_LOADER_H_

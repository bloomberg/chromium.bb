// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class AboutResource;
class ResourceList;
}  // namespace google_apis

namespace drive {

class JobScheduler;
class ResourceEntry;

namespace internal {

class ChangeList;
class ChangeListLoaderObserver;
class ChangeListProcessor;
class DirectoryFetchInfo;
class ResourceMetadata;

// Callback run as a response to SearchFromServer.
typedef base::Callback<void(ScopedVector<ChangeList> change_lists,
                            FileError error)> LoadChangeListCallback;

// ChangeListLoader is used to load the change list, the full resource list,
// and directory contents, from WAPI (codename for Documents List API)
// or Google Drive API.  The class also updates the resource metadata with
// the change list loaded from the server.
//
// Note that the difference between "resource list" and "change list" is
// subtle hence the two words are often used interchangeably. To be precise,
// "resource list" refers to metadata from the server when fetching the full
// resource metadata, or fetching directory contents, whereas "change list"
// refers to metadata from the server when fetching changes (delta).
class ChangeListLoader {
 public:
  ChangeListLoader(base::SequencedTaskRunner* blocking_task_runner,
                   ResourceMetadata* resource_metadata,
                   JobScheduler* scheduler);
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

  // Starts the change list loading first from the cache. If loading from the
  // cache is successful, runs |callback| immediately and starts checking
  // server for updates in background. If loading from the cache is
  // unsuccessful, starts loading from the server, and runs |callback| to tell
  // the result to the caller when it is finished.
  //
  // If |directory_fetch_info| is not empty, the directory will be fetched
  // first from the server, so the UI can show the directory contents
  // instantly before the entire change list loading is complete.
  //
  // |callback| must not be null.
  void LoadIfNeeded(const DirectoryFetchInfo& directory_fetch_info,
                    const FileOperationCallback& callback);

 private:
  // Starts the resource metadata loading and calls |callback| when it's
  // done. |directory_fetch_info| is used for fast fetch. If there is already
  // a loading job in-flight for |directory_fetch_info|, just append the
  // |callback| to the callback queue of the already running job.
  void Load(const DirectoryFetchInfo& directory_fetch_info,
            const FileOperationCallback& callback);

  // Part of Load(). DoInitialLoad() is called if it is the first time to Load.
  // Otherwise DoUpdateLoad() is used. The difference of two cases are:
  // - When we could load from cache, DoInitialLoad runs callback immediately
  //   and further operations (check changestamp and load from server if needed)
  //   in background.
  // - Even when |directory_fetch_info| is set, DoInitialLoad runs change list
  //   loading after directory loading is finished.
  void DoInitialLoad(const DirectoryFetchInfo& directory_fetch_info,
                     int64 local_changestamp);
  void DoUpdateLoad(const DirectoryFetchInfo& directory_fetch_info,
                    int64 local_changestamp);

  // Part of Load().
  // This function should be called when the change list load is complete.
  // Flushes the callbacks for change list loading and all directory loading.
  void OnChangeListLoadComplete(FileError error);

  // Part of Load().
  // This function should be called when the directory load is complete.
  // Flushes the callbacks waiting for the directory to be loaded.
  void OnDirectoryLoadComplete(const DirectoryFetchInfo& directory_fetch_info,
                               FileError error);

  // ================= Implementation for change list loading =================

  // Initiates the change list loading from the server when |local_changestamp|
  // is older than the server changestamp. If |directory_fetch_info| is set,
  // do directory loading before change list loading.
  void LoadFromServerIfNeeded(const DirectoryFetchInfo& directory_fetch_info,
                              int64 local_changestamp);

  // Part of LoadFromServerIfNeeded().
  // Called after GetAboutResource() for getting remote changestamp is complete.
  void LoadFromServerIfNeededAfterGetAbout(
      const DirectoryFetchInfo& directory_fetch_info,
      int64 local_changestamp,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of LoadFromServerIfNeeded().
  // When LoadFromServerIfNeeded is called with |directory_fetch_info| for a
  // specific directory, it tries to load the directory before loading the
  // content of full filesystem. This method is called after directory loading
  // is finished, and proceeds to the normal pass: LoadChangeListServer.
  void LoadFromServerIfNeededAfterLoadDirectory(
      const DirectoryFetchInfo& directory_fetch_info,
      scoped_ptr<google_apis::AboutResource> about_resource,
      int64 start_changestamp,
      FileError error);

  // Part of LoadFromServerIfNeeded().
  // Starts loading the change list since |start_changestamp|, or the full
  // resource list if |start_changestamp| is zero. For full update, the
  // largest_change_id and root_folder_id from |about_resource| will be used.
  void LoadChangeListFromServer(
      scoped_ptr<google_apis::AboutResource> about_resource,
      int64 start_changestamp);

  // Part of LoadChangeListFromServer().
  // Called when the entire change list is loaded.
  void LoadChangeListFromServerAfterLoadChangeList(
      scoped_ptr<google_apis::AboutResource> about_resource,
      bool is_delta_update,
      ScopedVector<ChangeList> change_lists,
      FileError error);

  // Part of LoadChangeListFromServer().
  // Called when the resource metadata is updated.
  void LoadChangeListFromServerAfterUpdate();

  // ================= Implementation for directory loading =================

  // Compares the directory's changestamp and |last_known_remote_changestamp_|.
  // Starts DoLoadDirectoryFromServer() if the local data is old and runs
  // |callback| when finished. If it is up to date, calls back immediately.
  void CheckChangestampAndLoadDirectoryIfNeeded(
      const DirectoryFetchInfo& directory_fetch_info,
      int64 local_changestamp,
      const FileOperationCallback& callback);

  // Loads the directory contents from server, and updates the local metadata.
  // Runs |callback| when it is finished.
  void DoLoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info,
                                 const FileOperationCallback& callback);

  // Part of DoLoadDirectoryFromServer() for the grand root ("/drive").
  void DoLoadGrandRootDirectoryFromServerAfterGetResourceEntryByPath(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      FileError error,
      scoped_ptr<ResourceEntry> entry);

  // Part of DoLoadDirectoryFromServer() for the grand root ("/drive").
  void DoLoadGrandRootDirectoryFromServerAfterGetAboutResource(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of DoLoadDirectoryFromServer() for a normal directory.
  void DoLoadDirectoryFromServerAfterLoad(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      ScopedVector<ChangeList> change_lists,
      FileError error);

  // Part of DoLoadDirectoryFromServer().
  void DoLoadDirectoryFromServerAfterRefresh(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      const base::FilePath* directory_path,
      FileError error);

  // ================= Implementation for other stuff =================

  // This function is used to handle pagenation for the result from
  // JobScheduler::GetChangeList/GetAllResourceList/ContinueGetResourceList/
  // GetResourceListInDirectory().
  //
  // After all the change lists are fetched, |callback| will be invoked with
  // the collected change lists.
  void OnGetChangeList(ScopedVector<ChangeList> change_lists,
                       const LoadChangeListCallback& callback,
                       base::TimeTicks start_time,
                       google_apis::GDataErrorCode status,
                       scoped_ptr<google_apis::ResourceList> resource_list);

  // Updates from the whole change list collected in |change_lists|.
  // Record file statistics as UMA histograms.
  //
  // See comments at ChangeListProcessor::Apply() for
  // |about_resource| and |is_delta_update|.
  // |callback| must not be null.
  void UpdateFromChangeList(
      scoped_ptr<google_apis::AboutResource> about_resource,
      ScopedVector<ChangeList> change_lists,
      bool is_delta_update,
      const base::Closure& callback);

  // Part of UpdateFromChangeList().
  // Called when ChangeListProcessor::Apply() is complete.
  // Notifies directory changes per the result of the change list processing.
  void UpdateFromChangeListAfterApply(
      ChangeListProcessor* change_list_processor,
      bool should_notify,
      base::Time start_time,
      const base::Closure& callback);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  ObserverList<ChangeListLoaderObserver> observers_;
  typedef std::map<std::string, std::vector<FileOperationCallback> >
      LoadCallbackMap;
  LoadCallbackMap pending_load_callback_;
  FileOperationCallback pending_update_check_callback_;

  // The last known remote changestamp. Used to check if a directory
  // changestamp is up-to-date for fast fetch.
  int64 last_known_remote_changestamp_;

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

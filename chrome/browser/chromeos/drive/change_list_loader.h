// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace google_apis {
class AboutResource;
class AppList;
class ResourceList;
}

namespace drive {

class ChangeList;
class ChangeListLoaderObserver;
class ChangeListProcessor;
class DriveWebAppsRegistry;
class JobScheduler;

// Callback run as a response to SearchFromServer.
typedef base::Callback<void(ScopedVector<ChangeList> change_lists,
                            FileError error)> LoadFeedListCallback;

// ChangeListLoader is used to load feeds from WAPI (codename for
// Documents List API) or Google Drive API and load the cached metadata.
class ChangeListLoader {
 public:
  ChangeListLoader(internal::ResourceMetadata* resource_metadata,
                   JobScheduler* scheduler,
                   DriveWebAppsRegistry* webapps_registry);
  ~ChangeListLoader();

  // Indicates whether there is a feed refreshing server request is in flight.
  bool IsRefreshing() const;

  // Adds and removes the observer.
  void AddObserver(ChangeListLoaderObserver* observer);
  void RemoveObserver(ChangeListLoaderObserver* observer);

  // Checks for updates on the server. Does nothing if the change list is now
  // being loaded or refreshed. |callback| must not be null.
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

  // Initiates the directory contents loading. This function first obtains
  // the changestamp from the server in order to set the per-directory
  // changestamp for the directory.
  //
  // Upon completion, |callback| is invoked. On success, the changestamp of
  // the directory is updated. |callback| must not be null.
  //
  // Note that This function initiates the loading without comparing the
  // directory changestamp against the server changestamp. The primary
  // purpose of this function is to update parts of entries in the directory
  // which can stale over time, such as thumbnail URLs.
  void LoadDirectoryFromServer(const std::string& directory_resource_id,
                               const FileOperationCallback& callback);

  // Starts retrieving search results for |search_query| from the server.
  // If |next_feed| is set, this is the feed url that will be fetched.
  // If |next_feed| is an empty string, the default URL is used.
  // Upon completion, |callback| is invoked.
  // |callback| must not be null.
  void SearchFromServer(const std::string& search_query,
                        const GURL& next_feed,
                        const LoadFeedListCallback& callback);

  // TODO(satorux): Make this private. crbug.com/232208
  // Updates whole directory structure feeds collected in |feed_list|.
  // Record file statistics as UMA histograms.
  //
  // See comments at ChangeListProcessor::ApplyFeeds() for
  // |about_resource| and |is_delta_feed|.
  // |callback| must not be null.
  void UpdateFromFeed(scoped_ptr<google_apis::AboutResource> about_resource,
                      ScopedVector<ChangeList> change_lists,
                      bool is_delta_feed,
                      const base::Closure& callback);

 private:
  // Start metadata loading of |directory_fetch_info|, and calls |callback|
  // when it's done. If there is already a loading job in-flight for
  // |directory_fetch_info|, just append the |callback| to the callback queue
  // of the already running job.
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
  void LoadChangeListFromServerAfterLoadDirectory(
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

  // Part of LoadFromServerIfNeeded().
  // Callback to fetch all the resource list response from the server.
  // After all the resource list are fetched, |callback|
  // will be invoked with the collected change lists.
  void OnGetResourceList(ScopedVector<ChangeList> change_lists,
                         const LoadFeedListCallback& callback,
                         base::TimeTicks start_time,
                         google_apis::GDataErrorCode status,
                         scoped_ptr<google_apis::ResourceList> data);

  // Part of LoadFromServerIfNeeded().
  // Applies the change list loaded from the server to local metadata storage.
  void UpdateMetadataFromFeedAfterLoadFromServer(
      scoped_ptr<google_apis::AboutResource> about_resource,
      bool is_delta_feed,
      ScopedVector<ChangeList> change_lists,
      FileError error);

  // Part of LoadFromServerIfNeeded().
  // Called when UpdateMetadataFromFeedAfterLoadFromServer is finished.
  void OnUpdateFromFeed();

  // ================= Implementation for directory loading =================

  // Part of LoadDirectoryFromServer().
  // Called after GetAboutResource() for getting remote changestamp is complete.
  // Note that it directly proceeds to DoLoadDirectoryFromServer() not going
  // through CheckChangestampAndLoadDirectoryIfNeeed, because the purpose of
  // LoadDirectoryFromServer is to force reloading regardless of changestamp.
  void LoadDirectoryFromServerAfterGetAbout(
      const std::string& directory_resource_id,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Compares the directory's changestamp and |last_known_remote_changestamp_|.
  // Starts DoLoadDirectoryFromServer() if the local data is old and runs
  // |callback| when finished. If it is up to date, calls back immediately.
  void CheckChangestampAndLoadDirectoryIfNeeed(
      const DirectoryFetchInfo& directory_fetch_info,
      int64 local_changestamp,
      const FileOperationCallback& callback);

  // Loads the directory contents from server, and updates the local metadata.
  // Runs |callback| when it is finished.
  void DoLoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info,
                                 const FileOperationCallback& callback);

  // Part of DoLoadDirectoryFromServer() for the grand root ("/drive").
  void DoLoadGrandRootDirectoryFromServerAfterGetEntryInfoByPath(
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
      FileError error,
      const base::FilePath& directory_path);

  // ================= Implementation for other stuff =================

  // Callback for handling response from |DriveAPIService::GetAppList|.
  // If the application list is successfully parsed, passes the list to
  // Drive webapps registry.
  void OnGetAppList(google_apis::GDataErrorCode status,
                    scoped_ptr<google_apis::AppList> app_list);

  // Part of SearchFromServer().
  // Processes the ResourceList received from server and passes to |callback|.
  void SearchFromServerAfterGetResourceList(
      const LoadFeedListCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceList> resource_list);

  // Part of UpdateFromFeed().
  // Callback for ChangeListProcessor::ApplyFeeds.
  void NotifyDirectoryChangedAfterApplyFeed(bool should_notify,
                                            base::Time start_time,
                                            const base::Closure& callback);

  internal::ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  DriveWebAppsRegistry* webapps_registry_;  // Not owned.
  ObserverList<ChangeListLoaderObserver> observers_;
  scoped_ptr<ChangeListProcessor> change_list_processor_;
  typedef std::map<std::string, std::vector<FileOperationCallback> >
      LoadCallbackMap;
  LoadCallbackMap pending_load_callback_;

  // Indicates whether there is a feed refreshing server request is in flight.
  int64 last_known_remote_changestamp_;

  // True if the file system feed is loaded from the cache or from the server.
  bool loaded_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChangeListLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChangeListLoader);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

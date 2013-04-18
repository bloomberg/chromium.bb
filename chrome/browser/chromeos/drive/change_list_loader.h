// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
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
class DriveScheduler;
class DriveWebAppsRegistry;

// Callback run as a response to SearchFromServer and LoadDirectoryFromServer.
typedef base::Callback<void(ScopedVector<ChangeList> change_lists,
                            DriveFileError error)> LoadFeedListCallback;

// ChangeListLoader is used to load feeds from WAPI (codename for
// Documents List API) or Google Drive API and load the cached metadata.
class ChangeListLoader {
 public:
  ChangeListLoader(DriveResourceMetadata* resource_metadata,
                   DriveScheduler* scheduler,
                   DriveWebAppsRegistry* webapps_registry);
  ~ChangeListLoader();

  // Adds and removes the observer.
  void AddObserver(ChangeListLoaderObserver* observer);
  void RemoveObserver(ChangeListLoaderObserver* observer);

  // Starts the change list loading first from the cache. If loading from the
  // cache is successful, runs |callback| and starts loading from the server
  // if needed (i.e. the cache is old). If loading from the cache is
  // unsuccessful, starts loading from the server, and runs |callback| to
  // tell the result to the caller.
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

  // Checks for updates on the server. Does nothing if the change list is now
  // being loaded or refreshed. |callback| must not be null.
  void CheckForUpdates(const FileOperationCallback& callback);

  // Updates whole directory structure feeds collected in |feed_list|.
  // Record file statistics as UMA histograms.
  //
  // See comments at ChangeListProcessor::ApplyFeeds() for
  // |about_resource| and |is_delta_feed|.
  // |update_finished_callback| must not be null.
  void UpdateFromFeed(scoped_ptr<google_apis::AboutResource> about_resource,
                      ScopedVector<ChangeList> change_lists,
                      bool is_delta_feed,
                      const base::Closure& update_finished_callback);

  // Indicates whether there is a feed refreshing server request is in flight.
  bool IsRefreshing() const;

 private:
  // Implementation of LoadIfNeeded and CheckForUpdates. Start metadata loading
  // of |directory_fetch_info|, and calls |callback| when it's done. If there is
  // already a loading job in-flight for |directory_fetch_info|, just append
  // the |callback| to the callback queue of the already running job.
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
  // Initiates the change list loading from the server when |local_changestamp|
  // is older than the server changestamp. If |directory_fetch_info| is set,
  // do directory loading before change list loading.
  void LoadFromServerIfNeeded(const DirectoryFetchInfo& directory_fetch_info,
                              int64 local_changestamp);

  // Part of Load().
  // Checks the directory's changestamp and |last_known_remote_changestamp_|,
  // and load the feed from server if it is old. Runs |callback| when it's done.
  void CheckChangestampAndLoadDirectoryIfNeeed(
      const DirectoryFetchInfo& directory_fetch_info,
      int64 local_changestamp,
      const FileOperationCallback& callback);

  // Part of LoadChangeListFromServer().
  // Callback to fetch all the resource list response from the server.
  // After all the resource list are fetched, |callback|
  // will be invoked with the collected change lists.
  // |callback| must not be null.
  void OnGetResourceList(
      ScopedVector<ChangeList> change_lists,
      const LoadFeedListCallback& callback,
      base::TimeTicks start_time,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceList> data);

  // Part of LoadDirectoryFromServer(). Called when
  // DriveScheduler::GetAboutResource() is complete. Calls
  // DoLoadDirectoryFromServer() to initiate the directory contents loading.
  void LoadDirectoryFromServerAfterGetAbout(
      const std::string& directory_resource_id,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Initiates the directory contents loading, based on |directory_fetch_info|.
  // When it is finished it just runs |callback| but no other callbacks in
  // |pending_load_callback_|, because it depends on the caller whether to flush
  // callbacks. Thus, the caller must be responsible for task flushing.
  void DoLoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info,
                                 const FileOperationCallback& callback);

  // Part of DoLoadDirectoryFromServer for the grand root ("/drive" directory).
  // Called when GetEntryInfoByPath is completed.
  // |callback| must not be null.
  void DoLoadGrandRootDirectoryFromServerAfterGetEntryInfoByPath(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of DoLoadDirectoryFromServer for the grand root ("/drive" directory).
  // Called when GetAboutResource is completed.
  // |callback| must not be null.
  void DoLoadGrandRootDirectoryFromServerAfterGetAboutResource(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of DoLoadDirectoryFromServer(). Called after
  // LoadFromServer() is complete.
  void DoLoadDirectoryFromServerAfterLoad(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      ScopedVector<ChangeList> change_lists,
      DriveFileError error);

  // Part of DoLoadDirectoryFromServer(). Called after
  // DriveResourceMetadata::RefreshDirectory() is complete.
  void DoLoadDirectoryFromServerAfterRefresh(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      DriveFileError error,
      const base::FilePath& directory_path);

  // Part of LoadFromServerIfNeeded(). Called when GetAboutResource is complete.
  void LoadFromServerIfNeededAfterGetAbout(
      const DirectoryFetchInfo& directory_fetch_info,
      int64 local_changestamp,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of LoadFromServerIfNeeded().
  // Starts loading the change list since |start_changestamp|, or the full
  // resource list if |start_changestamp| is zero. For full update, the
  // largest_change_id and root_folder_id from |about_resource| will be used.
  void LoadChangeListFromServer(
      scoped_ptr<google_apis::AboutResource> about_resource,
      int64 start_changestamp);

  // Part of LoadFromServerIfNeeded().
  // Starts loading the change list from the server. Called after the
  // directory contents are "fast-fetch"ed.
  void LoadChangeListFromServerAfterLoadDirectory(
    const DirectoryFetchInfo& directory_fetch_info,
    scoped_ptr<google_apis::AboutResource> about_resource,
    int64 start_changestamp,
    DriveFileError error);

  // Callback for handling response from |DriveAPIService::GetAppList|.
  // If the application list is successfully parsed, passes the list to
  // Drive webapps registry.
  void OnGetAppList(google_apis::GDataErrorCode status,
                    scoped_ptr<google_apis::AppList> app_list);

  // Part of SearchFromServer. Called when ResourceList is fetched from the
  // server.
  // |callback| must not be null.
  void SearchFromServerAfterGetResourceList(
      const LoadFeedListCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceList> resource_list);

  // Part of LoadChangeListFromServer().
  // Applies the change list loaded from the server to local metadata storage.
  void UpdateMetadataFromFeedAfterLoadFromServer(
      scoped_ptr<google_apis::AboutResource> about_resource,
      bool is_delta_feed,
      ScopedVector<ChangeList> change_lists,
      DriveFileError error);

  // Callback for ChangeListProcessor::ApplyFeeds.
  void NotifyDirectoryChangedAfterApplyFeed(
      bool should_notify,
      const base::Closure& update_finished_callback);

  // Part of LoadChangeListFromServer().
  // Called when UpdateMetadataFromFeedAfterLoadFromServer is finished.
  void OnUpdateFromFeed();

  // This function should be called when the change list load is complete.
  // Flushes the callbacks for change list loading and all directory loading.
  void OnChangeListLoadComplete(DriveFileError error);

  // This function should be called when the directory load is complete.
  // Flushes the callbacks waiting for the directory to be loaded.
  void OnDirectoryLoadComplete(const DirectoryFetchInfo& directory_fetch_info,
                               DriveFileError error);

  DriveResourceMetadata* resource_metadata_;  // Not owned.
  DriveScheduler* scheduler_;  // Not owned.
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

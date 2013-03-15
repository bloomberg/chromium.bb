// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

#include <queue>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

namespace base {
class Value;
}

namespace google_apis {
class AboutResource;
class AppList;
class ResourceList;
}

namespace drive {

class DriveCache;
class ChangeListLoaderObserver;
class ChangeListProcessor;
class DriveScheduler;
class DriveWebAppsRegistry;

// Callback run as a response to SearchFromServer and LoadDirectoryFromServer.
typedef base::Callback<
    void(const ScopedVector<google_apis::ResourceList>& feed_list,
         DriveFileError error)> LoadFeedListCallback;

// ChangeListLoader is used to load feeds from WAPI (codename for
// Documents List API) and load the cached proto file.
class ChangeListLoader {
 public:
  ChangeListLoader(
      DriveResourceMetadata* resource_metadata,
      DriveScheduler* scheduler,
      DriveWebAppsRegistry* webapps_registry,
      DriveCache* cache,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
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
  // TODO(satorux): Implement the "fast-fetch" behavior. crbug.com/178348
  //
  // |callback| must not be null.
  void Load(const DirectoryFetchInfo directory_fetch_info,
            const FileOperationCallback& callback);

  // Starts the change list loading from the cache, and runs |callback| to
  // tell the result to the caller.  |callback| must not be null.
  // TODO(satorux): make this private. crbug.com/193417
  void LoadFromCache(const FileOperationCallback& callback);

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
  void LoadDirectoryFromServer(
      const std::string& directory_resource_id,
      const FileOperationCallback& callback);

  // Starts retrieving search results for |search_query| from the server.
  // If |next_feed| is set, this is the feed url that will be fetched.
  // If |next_feed| is an empty string, the default URL is used.
  // If |shared_with_me| is true, it searches for the files shared to the user,
  // otherwise searches for the files owned by the user.
  // Upon completion, |feed_load_callback| is invoked.
  // |feed_load_callback| must not be null.
  void SearchFromServer(const std::string& search_query,
                        bool shared_with_me,
                        const GURL& next_feed,
                        const LoadFeedListCallback& feed_load_callback);

  // Initiates the chnage list loading from the server if the local
  // changestamp is older than the server changestamp.
  // See the comment at Load() for |directory_fetch_info| parameter.
  // |callback| must not be null.
  // TODO(satorux): make this private. crbug.com/193417
  void LoadFromServerIfNeeded(const DirectoryFetchInfo& directory_fetch_info,
                              const FileOperationCallback& callback);

  // Updates whole directory structure feeds collected in |feed_list|.
  // Record file statistics as UMA histograms.
  //
  // See comments at ChangeListProcessor::ApplyFeeds() for
  // |is_delta_feed| and |root_feed_changestamp|.
  // |update_finished_callback| must not be null.
  void UpdateFromFeed(const ScopedVector<google_apis::ResourceList>& feed_list,
                      bool is_delta_feed,
                      int64 root_feed_changestamp,
                      const base::Closure& update_finished_callback);

  // Schedules |callback| to run when it's ready (i.e. the change list
  // loading is complete or the directory specified by |directory_fetch_info|
  // is loaded). |directory_fetch_info| can be empty if the callback is not
  // interested in a particular directory.
  // |callback| must not be null.
  void ScheduleRun(const DirectoryFetchInfo& directory_fetch_info,
                   const FileOperationCallback& callback);

  // Indicates whether there is a feed refreshing server request is in flight.
  bool refreshing() const { return refreshing_; }

 private:
  struct GetResourceListUiState;
  struct LoadFeedParams;
  struct LoadRootFeedParams;

  // Part of Load(). Called after loading from the cache is complete.
  void LoadAfterLoadFromCache(
      const DirectoryFetchInfo directory_fetch_info,
      const FileOperationCallback& callback,
      DriveFileError error);

  // Starts loading from the server, with details specified in |params|. This
  // is a general purpuse function, which is used for loading change lists,
  // full resource lists, and directory contents.
  void LoadFromServer(scoped_ptr<LoadFeedParams> params,
                      const LoadFeedListCallback& callback);

  // Part of LoadFromServer. Called when DriveScheduler::GetResourceList() is
  // complete. |callback| must not be null.
  void LoadFromServerAfterGetResourceList(
      scoped_ptr<LoadFeedParams> params,
      const LoadFeedListCallback& callback,
      base::TimeTicks start_time,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceList> data);

  // Part of LoadDirectoryFromServer() Callled when
  // DriveScheduler::GetAboutResource() is complete. Calls
  // DoLoadDirectoryFromServer() to initiate the directory contents loading.
  void LoadDirectoryFromServerAfterGetAbout(
      const std::string& directory_resource_id,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Initiates the directory contents loading, based on
  // |directory_fetch_info|.
  void DoLoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info,
                                 const FileOperationCallback& callback);

  // Part of DoLoadDirectoryFromServer(). Called after
  // LoadFromServer() is complete.
  void DoLoadDirectoryFromServerAfterLoad(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      const ScopedVector<google_apis::ResourceList>& resource_list,
      DriveFileError error);

  // Part of DoLoadDirectoryFromServer(). Called after
  // DriveResourceMetadata::RefreshDirectory() is complete.
  void DoLoadDirectoryFromServerAfterRefresh(
      const FileOperationCallback& callback,
      DriveFileError error,
      const base::FilePath& directory_path);

  // Part of LoadFromCache(). Called after reading of proto is complete.
  void LoadFromCacheAfterReadProto(LoadRootFeedParams* params,
                                   DriveFileError error);

  // Part of LoadFromServerIfNeeded() Callled when
  // DriveScheduler::GetAboutResource() is complete. This method calls
  // CompareChangestampsAndLoadIfNeeded() to make a decision about whether or
  // not to fetch the change list.
  void LoadFromServerIfNeededAfterGetAbout(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Compares |remote_changestamp| and |local_changestamp| and triggers
  // LoadFromServer if necessary.
  void CompareChangestampsAndLoadIfNeeded(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      int64 remote_changestamp,
      int64 local_changestamp);

  // Starts loading the change list since |start_changestamp|, or the full
  // resource list if |start_changestamp| is zero. |remote_changestamp| will
  // be stored in DriveResourceMetadata, once loading is done.
  // callback must not be null.
  void LoadChangeListFromServer(
      int64 start_changestamp,
      int64 remote_changestamp,
      const FileOperationCallback& callback);

  // Callback for handling response from |DriveAPIService::GetAppList|.
  // If the application list is successfully parsed, passes the list to
  // Drive webapps registry.
  void OnGetAppList(google_apis::GDataErrorCode status,
                    scoped_ptr<google_apis::AppList> app_list);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindEntryByPath() request.
  void UpdateMetadataFromFeedAfterLoadFromServer(
      bool is_delta_feed,
      int64 feed_changestamp,
      const FileOperationCallback& callback,
      const ScopedVector<google_apis::ResourceList>& feed_list,
      DriveFileError error);

  // Save filesystem to disk.
  void SaveFileSystem();

  // Callback for handling UI updates caused by feed fetching.
  void OnNotifyResourceListFetched(
      base::WeakPtr<GetResourceListUiState> ui_state);

  // Callback for ChangeListProcessor::ApplyFeeds.
  void NotifyDirectoryChanged(bool should_notify,
                              const base::Closure& update_finished_callback);

  // Callback for UpdateFromFeed.
  void OnUpdateFromFeed(const FileOperationCallback& load_finished_callback);

  // This function should be called when the change list load is complete.
  // Runs |callback| with |error|, and flushes the queue.
  void OnChangeListLoadComplete(const FileOperationCallback& callback,
                                DriveFileError error);

  // Flushes the queue by running all the callbacks scheduled via
  // ScheduleRun(), with the given error code.
  void FlushQueue(DriveFileError error);

  DriveResourceMetadata* resource_metadata_;  // Not owned.
  DriveScheduler* scheduler_;  // Not owned.
  DriveWebAppsRegistry* webapps_registry_;  // Not owned.
  DriveCache* cache_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ObserverList<ChangeListLoaderObserver> observers_;
  scoped_ptr<ChangeListProcessor> change_list_processor_;
  typedef std::pair<DirectoryFetchInfo,
                    FileOperationCallback> QueuedCallbackInfo;
  std::queue<QueuedCallbackInfo> queue_;

  // Indicates whether there is a feed refreshing server request is in flight.
  bool refreshing_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChangeListLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChangeListLoader);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FEED_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FEED_LOADER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

class FilePath;

namespace base {
class Value;
}

namespace google_apis {
class DocumentFeed;
class DriveServiceInterface;
}

namespace drive {

class DriveCache;
class DriveFeedLoaderObserver;
class DriveFeedProcessor;
class DriveWebAppsRegistryInterface;
struct GetDocumentsUiState;
struct LoadFeedParams;

// Callback run as a response to LoadFromServer.
typedef base::Callback<void(scoped_ptr<LoadFeedParams> params,
                            DriveFileError error)>
    LoadDocumentFeedCallback;

// Set of parameters sent to LoadFromServer and LoadDocumentFeedCallback.
// Value of |start_changestamp| determines the type of feed to load - 0 means
// root feed, every other value would trigger delta feed.
// In the case of loading the root feed we use |root_feed_changestamp| as its
// initial changestamp value since it does not come with that info.
//
// Loaded feed may be partial due to size limit on a single feed. In that case,
// the loaded feed will have next feed url set. Iff |load_subsequent_feeds|
// parameter is set, feed loader will load all subsequent feeds.
//
// When all feeds are loaded, |feed_load_callback| is invoked with the retrieved
// feeds. Then |load_finished_callback| is invoked with the error code.
//
// If invoked as a part of content search, query will be set in |search_query|.
// If |feed_to_load| is set, this is feed url that will be used to load feed.
//
// |feed_load_callback| must not be null.
// |load_finished_callback| may be null.
struct LoadFeedParams {
  explicit LoadFeedParams(const LoadDocumentFeedCallback& feed_load_callback);

  ~LoadFeedParams();

  // Changestamps are positive numbers in increasing order. The difference
  // between two changestamps is proportional equal to number of items in
  // delta feed between them - bigger the difference, more likely bigger
  // number of items in delta feeds.
  int64 start_changestamp;
  int64 root_feed_changestamp;
  std::string search_query;
  bool shared_with_me;
  std::string directory_resource_id;
  GURL feed_to_load;
  bool load_subsequent_feeds;
  const LoadDocumentFeedCallback feed_load_callback;
  FileOperationCallback load_finished_callback;
  ScopedVector<google_apis::DocumentFeed> feed_list;
  scoped_ptr<GetDocumentsUiState> ui_state;
  // On initial feed load for Drive API, remember root ID for
  // DriveResourceData initialization later in UpdateFromFeed().
  std::string root_resource_id;
};

// Defines set of parameters sent to callback OnProtoLoaded().
struct LoadRootFeedParams {
  explicit LoadRootFeedParams(const FileOperationCallback& callback);
  ~LoadRootFeedParams();

  std::string proto;
  DriveFileError load_error;
  base::Time last_modified;
  // Time when filesystem began to be loaded from disk.
  base::Time load_start_time;
  const FileOperationCallback callback;
};

// DriveFeedLoader is used to load feeds from WAPI (codename for
// Documents List API) and load the cached proto file.
class DriveFeedLoader {
 public:
  DriveFeedLoader(
      DriveResourceMetadata* resource_metadata,
      google_apis::DriveServiceInterface* drive_service,
      DriveWebAppsRegistryInterface* webapps_registry,
      DriveCache* cache,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_);
  ~DriveFeedLoader();

  // Adds and removes the observer.
  void AddObserver(DriveFeedLoaderObserver* observer);
  void RemoveObserver(DriveFeedLoaderObserver* observer);

  // Starts root feed load from the cache, and runs |callback| to tell the
  // result to the caller.
  // |callback| must not be null.
  void LoadFromCache(const FileOperationCallback& callback);

  // Starts retrieving feed for a directory specified by |directory_resource_id|
  // from the server. Upon completion, |feed_load_callback| is invoked.
  // |feed_load_callback| must not be null.
  void LoadDirectoryFromServer(
      const std::string& directory_resource_id,
      const LoadDocumentFeedCallback& feed_load_callback);

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
                        const LoadDocumentFeedCallback& feed_load_callback);

  // Retrieves account metadata and determines from the last change timestamp
  // if the feed content loading from the server needs to be initiated.
  // |callback| must not be null.
  void ReloadFromServerIfNeeded(const FileOperationCallback& callback);

  // Updates whole directory structure feeds collected in |feed_list|.
  // Record file statistics as UMA histograms.
  //
  // See comments at DriveFeedProcessor::ApplyFeeds() for
  // |start_changestamp| and |root_feed_changestamp|.
  // |root_resource_id| is used for Drive API.
  // |update_finished_callback| must not be null.
  void UpdateFromFeed(
      const ScopedVector<google_apis::DocumentFeed>& feed_list,
      int64 start_changestamp,
      int64 root_feed_changestamp,
      const std::string& root_resource_id,
      const base::Closure& update_finished_callback);

  // Indicates whether there is a feed refreshing server request is in flight.
  bool refreshing() const { return refreshing_; }

 private:
  // Starts root feed load from the server, with details specified in |params|.
  void LoadFromServer(scoped_ptr<LoadFeedParams> params);

  // Callback for handling root directory refresh from the cache.
  void OnProtoLoaded(LoadRootFeedParams* params);

  // Continues handling root directory refresh after the resource metadata
  // is fully loaded.
  void ContinueWithInitializedResourceMetadata(LoadRootFeedParams* params,
                                               DriveFileError error);

  // Helper callback for handling results of metadata retrieval initiated from
  // ReloadFromServerIfNeeded(). This method makes a decision about fetching
  // the content of the root feed during the root directory refresh process.
  void OnGetAccountMetadata(const FileOperationCallback& callback,
                            google_apis::GDataErrorCode status,
                            scoped_ptr<base::Value> feed_data);

  // Callback for handling response from |DriveAPIService::GetApplicationInfo|.
  // If the application list is successfully parsed, passes the list to
  // Drive webapps registry.
  void OnGetApplicationList(google_apis::GDataErrorCode status,
                            scoped_ptr<base::Value> json);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindEntryByPath() request.
  void OnFeedFromServerLoaded(scoped_ptr<LoadFeedParams> params,
                              DriveFileError error);

  // Callback for handling response from |GDataWapiService::GetDocuments|.
  // Invokes |callback| when done.
  // |callback| must not be null.
  void OnGetDocuments(
      scoped_ptr<LoadFeedParams> params,
      base::TimeTicks start_time,
      google_apis::GDataErrorCode status,
      scoped_ptr<base::Value> data);

  // Callback for handling results of feed parse.
  void OnParseFeed(scoped_ptr<LoadFeedParams> params,
                   base::TimeTicks start_time,
                   scoped_ptr<google_apis::DocumentFeed>* current_feed);

  // Callback for handling response from |DriveAPIService::GetDocuments|.
  // Invokes |callback| when done.
  // |callback| must not be null.
  void OnGetChangelist(scoped_ptr<LoadFeedParams> params,
                       base::TimeTicks start_time,
                       google_apis::GDataErrorCode status,
                       scoped_ptr<base::Value> data);

  // Save filesystem to disk.
  void SaveFileSystem();

  // Callback for handling UI updates caused by document fetching.
  void OnNotifyDocumentFeedFetched(
      base::WeakPtr<GetDocumentsUiState> ui_state);

  // Callback for DriveFeedProcessor::ApplyFeeds.
  void NotifyDirectoryChanged(
      bool should_notify,
      const base::Closure& update_finished_callback);

  // Callback for UpdateFromFeed.
  void OnUpdateFromFeed(const FileOperationCallback& load_finished_callback);

  DriveResourceMetadata* resource_metadata_;  // Not owned.
  google_apis::DriveServiceInterface* drive_service_;  // Not owned.
  DriveWebAppsRegistryInterface* webapps_registry_;  // Not owned.
  DriveCache* cache_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ObserverList<DriveFeedLoaderObserver> observers_;
  scoped_ptr<DriveFeedProcessor> feed_processor_;

  // Indicates whether there is a feed refreshing server request is in flight.
  bool refreshing_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveFeedLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveFeedLoader);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FEED_LOADER_H_

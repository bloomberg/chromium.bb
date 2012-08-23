// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_LOADER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/gdata/drive_resource_metadata.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

namespace base {
class Value;
}

namespace gdata {

class DocumentFeed;
class DriveCache;
class DriveServiceInterface;
class DriveWebAppsRegistryInterface;
struct GetDocumentsUiState;

// Set of parameters sent to LoadDocumentFeedCallback callback.
struct GetDocumentsParams {
  GetDocumentsParams(int64 start_changestamp,
                     int64 root_feed_changestamp,
                     std::vector<DocumentFeed*>* feed_list,
                     const std::string& search_query,
                     const std::string& directory_resource_id,
                     const FileOperationCallback& callback,
                     GetDocumentsUiState* ui_state);
  ~GetDocumentsParams();

  // Changestamps are positive numbers in increasing order. The difference
  // between two changestamps is proportional equal to number of items in
  // delta feed between them - bigger the difference, more likely bigger
  // number of items in delta feeds.
  int64 start_changestamp;
  int64 root_feed_changestamp;
  scoped_ptr<std::vector<DocumentFeed*> > feed_list;
  std::string search_query;
  std::string directory_resource_id;
  FileOperationCallback callback;
  scoped_ptr<GetDocumentsUiState> ui_state;
};

// Defines set of parameters sent to callback OnProtoLoaded().
struct LoadRootFeedParams {
  LoadRootFeedParams(
        bool should_load_from_server,
        const FileOperationCallback& callback);
  ~LoadRootFeedParams();

  bool should_load_from_server;
  std::string proto;
  DriveFileError load_error;
  base::Time last_modified;
  // Time when filesystem began to be loaded from disk.
  base::Time load_start_time;
  const FileOperationCallback callback;
};

// Callback run as a response to LoadFromServer.
typedef base::Callback<void(GetDocumentsParams* params,
                            DriveFileError error)>
    LoadDocumentFeedCallback;

// GDataWapiFeedLoader is used to load feeds from WAPI (codename for
// Documents List API) and load the cached proto file.
class GDataWapiFeedLoader {
 public:
  // Used to notify events from the loader.
  // All events are notified on UI thread.
  class Observer {
   public:
    // Triggered when a content of a directory has been changed.
    // |directory_path| is a virtual directory path representing the
    // changed directory.
    virtual void OnDirectoryChanged(const FilePath& directory_path) {}

    // Triggered when a document feed is fetched. |num_accumulated_entries|
    // tells the number of entries fetched so far.
    virtual void OnDocumentFeedFetched(int num_accumulated_entries) {}

    // Triggered when the feed from the server is loaded.
    virtual void OnFeedFromServerLoaded() {}

   protected:
    virtual ~Observer() {}
  };

  GDataWapiFeedLoader(
      DriveResourceMetadata* resource_metadata,
      DriveServiceInterface* drive_service,
      DriveWebAppsRegistryInterface* webapps_registry,
      DriveCache* cache,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_);
  ~GDataWapiFeedLoader();

  // Adds and removes the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts root feed load from the cache. If successful, runs |callback| to
  // tell the caller that the loading was successful.
  //
  // Then, it will initiate retrieval of the root feed from the server unless
  // |should_load_from_server| is set to false. |should_load_from_server| is
  // false only for testing. If loading from the server is successful, runs
  // |callback| if it was not previously run (i.e. loading from the cache was
  // successful).
  //
  // |callback| may be null.
  void LoadFromCache(bool should_load_from_server,
                     const FileOperationCallback& callback);

  // Starts retrieving feed for a directory specified by |directory_resource_id|
  // from the server. Upon completion, |feed_load_callback| is invoked.
  // |feed_load_callback| must not be null.
  void LoadDirectoryFromServer(
      ContentOrigin initial_origin,
      const std::string& directory_resource_id,
      const LoadDocumentFeedCallback& feed_load_callback);

  // Starts retrieving search results for |search_query| from the server.
  // If |next_feed| is set, this is the feed url that will be fetched.
  // If |next_feed| is an empty string, the default URL is used.
  // Upon completion, |feed_load_callback| is invoked.
  // |feed_load_callback| must not be null.
  void SearchFromServer(ContentOrigin initial_origin,
                        const std::string& search_query,
                        const GURL& next_feed,
                        const LoadDocumentFeedCallback& feed_load_callback);

  // Retrieves account metadata and determines from the last change timestamp
  // if the feed content loading from the server needs to be initiated.
  void ReloadFromServerIfNeeded(
      ContentOrigin initial_origin,
      int64 local_changestamp,
      const FileOperationCallback& callback);

  // Updates whole directory structure feeds collected in |feed_list|.
  // On success, returns PLATFORM_FILE_OK. Record file statistics as UMA
  // histograms.
  //
  // See comments at GDataWapiFeedProcessor::ApplyFeeds() for
  // |start_changestamp| and |root_feed_changestamp|.
  DriveFileError UpdateFromFeed(
    const std::vector<DocumentFeed*>& feed_list,
    int64 start_changestamp,
    int64 root_feed_changestamp);

 private:
  struct LoadFeedParams;

  // Starts root feed load from the server, with detail specified in |param|.
  void LoadFromServer(const LoadFeedParams& param);

  // Callback for handling root directory refresh from the cache.
  void OnProtoLoaded(LoadRootFeedParams* params);

  // Continues handling root directory refresh after the directory service
  // is fully loaded.
  void ContinueWithInitializedDirectoryService(LoadRootFeedParams* params,
                                               DriveFileError error);

  // Helper callback for handling results of metadata retrieval initiated from
  // ReloadFeedFromServerIfNeeded(). This method makes a decision about fetching
  // the content of the root feed during the root directory refresh process.
  void OnGetAccountMetadata(
      ContentOrigin initial_origin,
      int64 local_changestamp,
      const FileOperationCallback& callback,
      GDataErrorCode status,
      scoped_ptr<base::Value> feed_data);

  // Helper callback for handling results of account data retrieval initiated
  // from ReloadFeedFromServerIfNeeded() for Drive V2 API.
  // This method makes a decision about fetching the content of the root feed
  // during the root directory refresh process.
  void OnGetAboutResource(
      ContentOrigin initial_origin,
      int64 local_changestamp,
      const FileOperationCallback& callback,
      GDataErrorCode status,
      scoped_ptr<base::Value> feed_data);

  // Callback for handling response from |DriveAPIService::GetApplicationInfo|.
  // If the application list is successfully parsed, passes the list to
  // Drive webapps registry.
  void OnGetApplicationList(GDataErrorCode status,
                            scoped_ptr<base::Value> json);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindEntryByPath() request.
  void OnFeedFromServerLoaded(GetDocumentsParams* params,
                              DriveFileError error);

  // Callback for handling response from |GDataWapiService::GetDocuments|.
  // Invokes |callback| when done.
  // |callback| must not be null.
  void OnGetDocuments(
      ContentOrigin initial_origin,
      const LoadDocumentFeedCallback& callback,
      GetDocumentsParams* params,
      base::TimeTicks start_time,
      GDataErrorCode status,
      scoped_ptr<base::Value> data);

  // Callback for handling response from |DriveAPIService::GetChanglist|.
  // Invokes |callback| when done.
  // |callback| must not be null.
  void OnGetChangelist(ContentOrigin initial_origin,
                       const LoadDocumentFeedCallback& callback,
                       GetDocumentsParams* params,
                       base::TimeTicks start_time,
                       GDataErrorCode status,
                       scoped_ptr<base::Value> data);

  // Save filesystem to disk.
  void SaveFileSystem();

  // Callback for handling UI updates caused by document fetching.
  void OnNotifyDocumentFeedFetched(
      base::WeakPtr<GetDocumentsUiState> ui_state);

  DriveResourceMetadata* resource_metadata_;  // Not owned.
  DriveServiceInterface* drive_service_;  // Not owned.
  DriveWebAppsRegistryInterface* webapps_registry_;  // Not owned.
  DriveCache* cache_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GDataWapiFeedLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(GDataWapiFeedLoader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_LOADER_H_

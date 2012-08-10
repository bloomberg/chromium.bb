// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_LOADER_H_

#include "base/callback.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"

namespace gdata {

class DriveWebAppsRegistryInterface;
class DocumentsServiceInterface;
class GDataCache;
struct GetDocumentsUiState;

// Set of parameters sent to LoadDocumentFeedCallback callback.
struct GetDocumentsParams {
  GetDocumentsParams(int64 start_changestamp,
                     int64 root_feed_changestamp,
                     std::vector<DocumentFeed*>* feed_list,
                     bool should_fetch_multiple_feeds,
                     const FilePath& search_file_path,
                     const std::string& search_query,
                     const std::string& directory_resource_id,
                     const FindEntryCallback& callback,
                     GetDocumentsUiState* ui_state);
  ~GetDocumentsParams();

  // Changestamps are positive numbers in increasing order. The difference
  // between two changestamps is proportional equal to number of items in
  // delta feed between them - bigger the difference, more likely bigger
  // number of items in delta feeds.
  int64 start_changestamp;
  int64 root_feed_changestamp;
  scoped_ptr<std::vector<DocumentFeed*> > feed_list;
  // Should we stop after getting first feed chunk, even if there is more
  // data.
  bool should_fetch_multiple_feeds;
  FilePath search_file_path;
  std::string search_query;
  std::string directory_resource_id;
  FindEntryCallback callback;
  scoped_ptr<GetDocumentsUiState> ui_state;
};

// Defines set of parameters sent to callback OnProtoLoaded().
struct LoadRootFeedParams {
  LoadRootFeedParams(
        FilePath search_file_path,
        bool should_load_from_server,
        const FindEntryCallback& callback);
  ~LoadRootFeedParams();

  FilePath search_file_path;
  bool should_load_from_server;
  std::string proto;
  GDataFileError load_error;
  base::Time last_modified;
  // Time when filesystem began to be loaded from disk.
  base::Time load_start_time;
  const FindEntryCallback callback;
};

// Callback run as a response to LoadFromServer.
typedef base::Callback<void(GetDocumentsParams* params,
                            GDataFileError error)>
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
      GDataDirectoryService* directory_service,
      DocumentsServiceInterface* documents_service,
      DriveWebAppsRegistryInterface* webapps_registry,
      GDataCache* cache,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_);
  ~GDataWapiFeedLoader();

  // Adds and removes the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts root feed load from the cache. If successful, it will try to find
  // the file upon retrieval completion. In addition to that, it will
  // initiate retrieval of the root feed from the server unless
  // |should_load_from_server| is set to false. |should_load_from_server| is
  // false only for testing.
  void LoadFromCache(bool should_load_from_server,
                     const FilePath& search_file_path,
                     const FindEntryCallback& callback);

  // Starts root feed load from the server. Value of |start_changestamp|
  // determines the type of feed to load - 0 means root feed, every other
  // value would trigger delta feed.
  // In the case of loading the root feed we use |root_feed_changestamp| as its
  // initial changestamp value since it does not come with that info.
  // When done |load_feed_callback| is invoked.
  // |entry_found_callback| is used only when this is invoked while searching
  // for file info, and is used in |load_feed_callback|. If successful, it will
  // try to find the file upon retrieval completion.
  // |should_fetch_multiple_feeds| is true iff don't want to stop feed loading
  // after we retrieve first feed chunk.
  // If invoked as a part of content search, query will be set in
  // |search_query|.
  // If |feed_to_load| is set, this is feed url that will be used to load feed.
  void LoadFromServer(
      ContentOrigin initial_origin,
      int64 start_changestamp,
      int64 root_feed_changestamp,
      bool should_fetch_multiple_feeds,
      const FilePath& search_file_path,
      const std::string& search_query,
      const GURL& feed_to_load,
      const std::string& directory_resource_id,
      const FindEntryCallback& entry_found_callback,
      const LoadDocumentFeedCallback& feed_load_callback);

  // Retrieves account metadata and determines from the last change timestamp
  // if the feed content loading from the server needs to be initiated.
  void ReloadFromServerIfNeeded(
      ContentOrigin initial_origin,
      int64 local_changestamp,
      const FilePath& search_file_path,
      const FindEntryCallback& callback);

  // Updates whole directory structure feeds collected in |feed_list|.
  // On success, returns PLATFORM_FILE_OK. Record file statistics as UMA
  // histograms.
  //
  // See comments at GDataWapiFeedProcessor::ApplyFeeds() for
  // |start_changestamp| and |root_feed_changestamp|.
  GDataFileError UpdateFromFeed(
    const std::vector<DocumentFeed*>& feed_list,
    int64 start_changestamp,
    int64 root_feed_changestamp);

 private:
  // Callback for handling root directory refresh from the cache.
  void OnProtoLoaded(LoadRootFeedParams* params);

  // Continues handling root directory refresh after the directory service
  // is fully loaded.
  void ContinueWithInitializedDirectoryService(LoadRootFeedParams* params,
                                               GDataFileError error);

  // Helper callback for handling results of metadata retrieval initiated from
  // ReloadFeedFromServerIfNeeded(). This method makes a decision about fetching
  // the content of the root feed during the root directory refresh process.
  void OnGetAccountMetadata(
      ContentOrigin initial_origin,
      int64 local_changestamp,
      const FilePath& search_file_path,
      const FindEntryCallback& callback,
      GDataErrorCode status,
      scoped_ptr<base::Value> feed_data);

  // Helper callback for handling results of account data retrieval initiated
  // from ReloadFeedFromServerIfNeeded() for Drive V2 API.
  // This method makes a decision about fetching the content of the root feed
  // during the root directory refresh process.
  void OnGetAboutResource(
      ContentOrigin initial_origin,
      int64 local_changestamp,
      const FilePath& search_file_path,
      const FindEntryCallback& callback,
      GDataErrorCode status,
      scoped_ptr<base::Value> feed_data);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindEntryByPath() request.
  void OnFeedFromServerLoaded(GetDocumentsParams* params,
                              GDataFileError error);

  // Callback for handling response from |GDataDocumentsService::GetDocuments|.
  // Invokes |callback| when done.
  void OnGetDocuments(
      ContentOrigin initial_origin,
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

  GDataDirectoryService* directory_service_;  // Not owned.
  DocumentsServiceInterface* documents_service_;  // Not owned.
  DriveWebAppsRegistryInterface* webapps_registry_;  // Not owned.
  GDataCache* cache_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ObserverList<Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GDataWapiFeedLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(GDataWapiFeedLoader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_FEED_LOADER_H_

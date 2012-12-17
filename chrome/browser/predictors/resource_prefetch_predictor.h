// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/predictors/resource_prefetcher.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/resource_type.h"

class PredictorsHandler;
class Profile;

namespace content {
class WebContents;
}

namespace net {
class URLRequest;
}

namespace predictors {

class ResourcePrefetcherManager;

// Contains logic for learning what can be prefetched and for kicking off
// speculative prefetching.
// - The class is a profile keyed service owned by the profile.
// - All the non-static methods of this class need to be called on the UI
//   thread.
//
// The overall flow of the resource prefetching algorithm is as follows:
//
// * ResourcePrefetchPredictorObserver - Listens for URL requests, responses and
//   redirects on the IO thread(via RDHostDelegate) and post tasks to the
//   ResourcePrefetchPredictor on the UI thread. This is owned by the
//   ProfileIOData for the profile.
// * ResourcePrefetchPredictorTables - Persists ResourcePrefetchPredictor data
//   to a sql database. Runs entirely on the DB thread. Owned by the
//   PredictorDatabase.
// * ResourcePrefetchPredictor - Learns about resource requirements per URL in
//   the UI thread through the ResourcePrefetchPredictorObserver and perisists
//   it to disk in the DB thread through the ResourcePrefetchPredictorTables. It
//   initiates resource prefetching using the ResourcePrefetcherManager. Owned
//   by profile.
// * ResourcePrefetcherManager - Manages the ResourcePrefetchers that do the
//   prefetching on the IO thread. The manager is owned by the
//   ResourcePrefetchPredictor and interfaces between the predictor on the UI
//   thread and the prefetchers on the IO thread.
// * ResourcePrefetcher - Lives entirely on the IO thread, owned by the
//   ResourcePrefetcherManager, and issues net::URLRequest to fetch resources.
//
// TODO(shishir): Do speculative prefetching for https resources and/or https
// main frame urls.
class ResourcePrefetchPredictor
    : public ProfileKeyedService,
      public content::NotificationObserver,
      public base::SupportsWeakPtr<ResourcePrefetchPredictor> {
 public:
  // Stores the data that we need to get from the URLRequest.
  struct URLRequestSummary {
    URLRequestSummary();
    URLRequestSummary(const URLRequestSummary& other);
    ~URLRequestSummary();

    NavigationID navigation_id;
    GURL resource_url;
    ResourceType::Type resource_type;

    // Only for responses.
    std::string mime_type;
    bool was_cached;
    GURL redirect_url;  // Empty unless request was redirected to a valid url.
  };

  ResourcePrefetchPredictor(const ResourcePrefetchPredictorConfig& config,
                            Profile* profile);
  virtual ~ResourcePrefetchPredictor();

  // Thread safe.
  static bool ShouldRecordRequest(net::URLRequest* request,
                                  ResourceType::Type resource_type);
  static bool ShouldRecordResponse(net::URLRequest* response);
  static bool ShouldRecordRedirect(net::URLRequest* response);

  // Determines the ResourceType from the mime type, defaulting to the
  // |fallback| if the ResourceType could not be determined.
  static ResourceType::Type GetResourceTypeFromMimeType(
      const std::string& mime_type,
      ResourceType::Type fallback);

  // 'ResourcePrefetchPredictorObserver' calls the below functions to inform the
  // predictor of main frame and resource requests. Should only be called if the
  // corresponding Should* functions return true.
  void RecordURLRequest(const URLRequestSummary& request);
  void RecordUrlResponse(const URLRequestSummary& response);
  void RecordUrlRedirect(const URLRequestSummary& response);

  // Called by ResourcePrefetcherManager to notify that prefetching has finished
  // for a navigation. Should take ownership of |requests|.
  virtual void FinishedPrefetchForNavigation(
      const NavigationID& navigation_id,
      PrefetchKeyType key_type,
      ResourcePrefetcher::RequestVector* requests);

 private:
  friend class ::PredictorsHandler;
  friend class ResourcePrefetchPredictorTest;

  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, DeleteUrls);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           LazilyInitializeEmpty);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           LazilyInitializeWithData);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           NavigationNotRecorded);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, NavigationUrlInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, NavigationUrlNotInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           NavigationUrlNotInDBAndDBFull);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, OnMainFrameRequest);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, OnMainFrameRedirect);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           OnSubresourceResponse);

  enum InitializationState {
    NOT_INITIALIZED = 0,
    INITIALIZING = 1,
    INITIALIZED = 2
  };

  // Stores prefetching results.
  struct Result {
    // Takes ownership of requests.
    Result(PrefetchKeyType key_type,
           ResourcePrefetcher::RequestVector* requests);
    ~Result();

    PrefetchKeyType key_type;
    scoped_ptr<ResourcePrefetcher::RequestVector> requests;

   private:
    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  typedef ResourcePrefetchPredictorTables::ResourceRow ResourceRow;
  typedef ResourcePrefetchPredictorTables::ResourceRows ResourceRows;
  typedef ResourcePrefetchPredictorTables::PrefetchData PrefetchData;
  typedef ResourcePrefetchPredictorTables::PrefetchDataMap PrefetchDataMap;
  typedef std::map<NavigationID, linked_ptr<std::vector<URLRequestSummary> > >
      NavigationMap;
  typedef std::map<NavigationID, Result*> ResultsMap;

  // Returns true if the main page request is supported for prediction.
  static bool IsHandledMainPage(net::URLRequest* request);

  // Returns true if the subresource request is supported for prediction.
  static bool IsHandledSubresource(net::URLRequest* request);

  // Returns true if the request (should have a response in it) is cacheable.
  static bool IsCacheable(const net::URLRequest* request);

  // content::NotificationObserver methods OVERRIDE.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ProfileKeyedService methods OVERRIDE.
  virtual void Shutdown() OVERRIDE;

  // Functions called on different network events pertaining to the loading of
  // main frame resource or sub resources.
  void OnMainFrameRequest(const URLRequestSummary& request);
  void OnMainFrameResponse(const URLRequestSummary& response);
  void OnMainFrameRedirect(const URLRequestSummary& response);
  void OnSubresourceResponse(const URLRequestSummary& response);
  void OnSubresourceLoadedFromMemory(const NavigationID& navigation_id,
                                     const GURL& resource_url,
                                     const std::string& mime_type,
                                     ResourceType::Type resource_type);

  // Called when onload completes for a navigation. We treat this point as the
  // "completion" of the navigation. The resources requested by the page upto
  // this point are the only ones considered for prefetching.
  void OnNavigationComplete(const NavigationID& navigation_id);

  // Returns true if there is PrefetchData that can be used for the
  // navigation and fills in the |prefetch_data| to resources that need to be
  // prefetched.
  bool GetPrefetchData(const NavigationID& navigation_id,
                       ResourcePrefetcher::RequestVector* prefetch_requests,
                       PrefetchKeyType* key_type);

  // Converts a PrefetchData into a ResourcePrefetcher::RequestVector.
  void PopulatePrefetcherRequest(const PrefetchData& data,
                                 ResourcePrefetcher::RequestVector* requests);

  // Starts prefetching if it is enabled and prefetching data exists for the
  // NavigationID either at the URL or at the host level.
  void StartPrefetching(const NavigationID& navigation_id);

  // Stops prefetching that may be in progress corresponding to |navigation_id|.
  void StopPrefetching(const NavigationID& navigation_id);

  // Starts initialization by posting a task to the DB thread to read the
  // predictor database.
  void StartInitialization();

  // Callback for task to read predictor database. Takes ownership of
  // |url_data_map| and |host_data_map|.
  void CreateCaches(scoped_ptr<PrefetchDataMap> url_data_map,
                    scoped_ptr<PrefetchDataMap> host_data_map);

  // Called during initialization when history is read and the predictor
  // database has been read.
  void OnHistoryAndCacheLoaded();

  // Removes data for navigations where the onload never fired. Will cleanup
  // inflight_navigations_ and results_map_.
  void CleanupAbandonedNavigations(const NavigationID& navigation_id);

  // Deletes all URLs from the predictor database, the caches and removes all
  // inflight navigations.
  void DeleteAllUrls();

  // Deletes data for the input |urls| and their corresponding hosts from the
  // predictor database and caches.
  void DeleteUrls(const history::URLRows& urls);

  // Callback for GetUrlVisitCountTask.
  void OnVisitCountLookup(int visit_count,
                          const NavigationID& navigation_id,
                          const std::vector<URLRequestSummary>& requests);

  // Removes the oldest entry in the input |data_map|, also deleting it from the
  // predictor database.
  void RemoveOldestEntryInPrefetchDataMap(PrefetchKeyType key_type,
                                          PrefetchDataMap* data_map);

  // Merges resources in |new_resources| into the |data_map| and correspondingly
  // updates the predictor database.
  void LearnNavigation(const std::string& key,
                       PrefetchKeyType key_type,
                       const std::vector<URLRequestSummary>& new_resources,
                       int max_data_map_size,
                       PrefetchDataMap* data_map);

  // Reports accuracy by comparing prefetched resources with resources that are
  // actually used by the page.
  void ReportAccuracyStats(PrefetchKeyType key_type,
                           const std::vector<URLRequestSummary>& actual,
                           ResourcePrefetcher::RequestVector* prefetched) const;

  // Reports predicted accuracy i.e. by comparing resources that are actually
  // used by the page with those that may have been prefetched.
  void ReportPredictedAccuracyStats(
      PrefetchKeyType key_type,
      const std::vector<URLRequestSummary>& actual,
      const ResourcePrefetcher::RequestVector& predicted) const;
  void ReportPredictedAccuracyStatsHelper(
      PrefetchKeyType key_type,
      const ResourcePrefetcher::RequestVector& predicted,
      const std::map<GURL, bool>& actual,
      int total_resources_fetched_from_network,
      int max_assumed_prefetched) const;

  // Used for testing to inject mock tables.
  void set_mock_tables(scoped_refptr<ResourcePrefetchPredictorTables> tables) {
    tables_ = tables;
  }

  Profile* const profile_;
  ResourcePrefetchPredictorConfig const config_;
  InitializationState initialization_state_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;
  scoped_refptr<ResourcePrefetcherManager> prefetch_manager_;
  content::NotificationRegistrar notification_registrar_;
  CancelableRequestConsumer history_lookup_consumer_;

  // Map of all the navigations in flight to their resource requests.
  NavigationMap inflight_navigations_;

  // Copy of the data in the predictor tables.
  scoped_ptr<PrefetchDataMap> url_table_cache_;
  scoped_ptr<PrefetchDataMap> host_table_cache_;

  ResultsMap results_map_;
  STLValueDeleter<ResultsMap> results_map_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictor);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_

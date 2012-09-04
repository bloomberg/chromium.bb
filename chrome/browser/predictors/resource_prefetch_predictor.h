// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/history/history_types.h"
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
//   it to disk in the DB thread through the ResourcePrefetchPredictorTables.
//   Owned by profile.
//
// TODO(shishir): Implement the prefetching of resources.
// TODO(shishir): Do speculative prefetching for https resources and/or https
// main_frame urls.
class ResourcePrefetchPredictor
    : public ProfileKeyedService,
      public content::NotificationObserver,
      public base::SupportsWeakPtr<ResourcePrefetchPredictor> {
 public:
  // The following config allows us to change the predictor constants and run
  // field trials with different constants.
  struct Config {
    // Initializes the config with default values.
    Config();

    // If a navigation hasn't seen a load complete event in this much time, it
    // is considered abandoned.
    int max_navigation_lifetime_seconds;  // Default 60
    // Size of LRU caches for the Url data.
    size_t max_urls_to_track;  // Default 500
    // The number of times, we should have seen a visit to this Url in history
    // to start tracking it. This is to ensure we dont bother with oneoff
    // entries.
    int min_url_visit_count;  // Default 3
    // The maximum number of resources to store per entry.
    int max_resources_per_entry;  // Default 50
    // The number of consecutive misses after we stop tracking a resource Url.
    int max_consecutive_misses;  // Default 3
  };

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

  ResourcePrefetchPredictor(const Config& config, Profile* profile);
  virtual ~ResourcePrefetchPredictor();

  // Thread safe.
  static bool IsEnabled(Profile* profile);
  static bool ShouldRecordRequest(net::URLRequest* request,
                                  ResourceType::Type resource_type);
  static bool ShouldRecordResponse(net::URLRequest* response);
  static bool ShouldRecordRedirect(net::URLRequest* response);

  static ResourceType::Type GetResourceTypeFromMimeType(
      const std::string& mime_type,
      ResourceType::Type fallback);

  void RecordURLRequest(const URLRequestSummary& request);
  void RecordUrlResponse(const URLRequestSummary& response);
  void RecordUrlRedirect(const URLRequestSummary& response);

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

  // TODO(shishir): Maybe use pointers to make the sort cheaper.
  typedef ResourcePrefetchPredictorTables::UrlTableRow UrlTableRow;
  typedef std::vector<UrlTableRow> UrlTableRowVector;

  enum InitializationState {
    NOT_INITIALIZED = 0,
    INITIALIZING = 1,
    INITIALIZED = 2
  };

  struct UrlTableCacheValue {
    UrlTableCacheValue();
    ~UrlTableCacheValue();

    UrlTableRowVector rows;
    base::Time last_visit;
  };

  typedef std::map<NavigationID, std::vector<URLRequestSummary> > NavigationMap;
  typedef std::map<GURL, UrlTableCacheValue> UrlTableCacheMap;

  // content::NotificationObserver methods OVERRIDE.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  static bool IsHandledMainPage(net::URLRequest* request);
  static bool IsHandledSubresource(net::URLRequest* response);
  static bool IsCacheable(const net::URLRequest* response);

  // Deal with different kinds of requests.
  void OnMainFrameRequest(const URLRequestSummary& request);
  void OnMainFrameResponse(const URLRequestSummary& response);
  void OnMainFrameRedirect(const URLRequestSummary& response);
  void OnSubresourceResponse(const URLRequestSummary& response);
  void OnSubresourceLoadedFromMemory(const NavigationID& navigation_id,
                                     const GURL& resource_url,
                                     const std::string& mime_type,
                                     ResourceType::Type resource_type);

  // Initialization code.
  void LazilyInitialize();
  void OnHistoryAndCacheLoaded();
  void CreateCaches(std::vector<UrlTableRow>* url_rows);

  // Database and cache cleanup code.
  void RemoveAnEntryFromUrlDB();
  void DeleteAllUrls();
  void DeleteUrls(const history::URLRows& urls);

  bool ShouldTrackUrl(const GURL& url);
  void CleanupAbandonedNavigations(const NavigationID& navigation_id);
  void OnNavigationComplete(const NavigationID& navigation_id);
  void LearnUrlNavigation(const GURL& main_frame_url,
                          const std::vector<URLRequestSummary>& new_value);
  void MaybeReportAccuracyStats(const NavigationID& navigation_id) const;
  void ReportAccuracyHistograms(const UrlTableRowVector& predicted,
                                const std::map<GURL, bool>& actual_resources,
                                int total_resources_fetched_from_network,
                                int max_assumed_prefetched) const;

  void SetTablesForTesting(
      scoped_refptr<ResourcePrefetchPredictorTables> tables);

  Profile* const profile_;
  Config config_;
  InitializationState initialization_state_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;
  content::NotificationRegistrar notification_registrar_;

  NavigationMap inflight_navigations_;
  UrlTableCacheMap url_table_cache_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictor);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_

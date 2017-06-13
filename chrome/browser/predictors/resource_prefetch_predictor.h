// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/glowplug_key_value_data.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/predictors/resource_prefetcher.h"
#include "chrome/browser/predictors/resource_prefetcher_manager.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/precache/content/precache_manager.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

class PredictorsHandler;
class Profile;

namespace net {
class URLRequest;
}

namespace predictors {

namespace internal {
constexpr char kResourcePrefetchPredictorPrefetchingDurationHistogram[] =
    "ResourcePrefetchPredictor.PrefetchingDuration";

const uint32_t kVersionedRemovedExperiment = 0x03ff25e3;
const uint32_t kUnusedRemovedExperiment = 0xf7f77166;
const uint32_t kNoStoreRemovedExperiment = 0xd90a199a;

struct ManifestCompare {
  bool operator()(const precache::PrecacheManifest& lhs,
                  const precache::PrecacheManifest& rhs) const;
};

struct LastVisitTimeCompare {
  template <typename T>
  bool operator()(const T& lhs, const T& rhs) const {
    return lhs.last_visit_time() < rhs.last_visit_time();
  }
};

}  // namespace internal

class TestObserver;
class ResourcePrefetcherManager;
class LoadingStatsCollector;

// Contains logic for learning what can be prefetched and for kicking off
// speculative prefetching.
// - The class is a profile keyed service owned by the profile.
// - All the non-static methods of this class need to be called on the UI
//   thread.
//
// The overall flow of the resource prefetching algorithm is as follows:
//
// * LoadingPredictorObserver - Listens for URL requests, responses and
//   redirects (client-side redirects are not supported) on the IO thread (via
//   ResourceDispatcherHostDelegate) and posts tasks to the
//   LoadingDataCollector on the UI thread. This is owned by the ProfileIOData
//   for the profile.
// * ResourcePrefetchPredictorTables - Persists ResourcePrefetchPredictor data
//   to a sql database. Runs entirely on the DB thread. Owned by the
//   PredictorDatabase.
// * ResourcePrefetchPredictor - Learns about resource requirements per URL in
//   the UI thread through the LoadingPredictorObserver and persists
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
// TODO(zhenw): Currently only main frame requests/redirects/responses are
// recorded. Consider recording sub-frame responses independently or together
// with main frame.
class ResourcePrefetchPredictor
    : public history::HistoryServiceObserver,
      public precache::PrecacheManager::Delegate {
 public:
  // Data collected for origin-based prediction, for a single origin during a
  // page load (see PageRequestSummary).
  struct OriginRequestSummary {
    OriginRequestSummary();
    OriginRequestSummary(const OriginRequestSummary& other);
    ~OriginRequestSummary();

    GURL origin;
    bool always_access_network;
    bool accessed_network;
    int first_occurrence;
  };

  // Stores the data that we need to get from the URLRequest.
  struct URLRequestSummary {
    URLRequestSummary();
    URLRequestSummary(const URLRequestSummary& other);
    ~URLRequestSummary();

    NavigationID navigation_id;
    GURL resource_url;
    GURL request_url;  // URL after all redirects.
    content::ResourceType resource_type;
    net::RequestPriority priority;
    base::TimeTicks response_time;
    bool before_first_contentful_paint;

    // Only for responses.
    std::string mime_type;
    bool was_cached;
    GURL redirect_url;  // Empty unless request was redirected to a valid url.

    bool has_validators;
    bool always_revalidate;
    bool is_no_store;
    bool network_accessed;

    // Initializes a |URLRequestSummary| from a |URLRequest| response.
    // Returns true for success. Note: NavigationID is NOT initialized
    // by this function.
    static bool SummarizeResponse(const net::URLRequest& request,
                                  URLRequestSummary* summary);
  };

  // Stores the data learned from a single navigation.
  struct PageRequestSummary {
    explicit PageRequestSummary(const GURL& main_frame_url);
    PageRequestSummary(const PageRequestSummary& other);
    ~PageRequestSummary();

    GURL main_frame_url;
    GURL initial_url;
    base::TimeTicks first_contentful_paint;

    // Stores all subresource requests within a single navigation, from initial
    // main frame request to navigation completion.
    std::vector<URLRequestSummary> subresource_requests;
    // Map of origin -> OriginRequestSummary. Only one instance of each origin
    // is kept per navigation, but the summary is updated several times.
    std::map<GURL, OriginRequestSummary> origins;
  };

  // Stores a result of prediction. Essentially, |subresource_urls| is main
  // result and other fields are used for diagnosis and histograms reporting.
  struct Prediction {
    Prediction();
    Prediction(const Prediction& other);
    ~Prediction();

    bool is_host;
    bool is_redirected;
    std::string main_frame_key;
    std::vector<GURL> subresource_urls;
  };

  // Used for reporting redirect prediction success/failure in histograms.
  // NOTE: This enumeration is used in histograms, so please do not add entries
  // in the middle.
  enum class RedirectStatus {
    NO_REDIRECT,
    NO_REDIRECT_BUT_PREDICTED,
    REDIRECT_NOT_PREDICTED,
    REDIRECT_WRONG_PREDICTED,
    REDIRECT_CORRECTLY_PREDICTED,
    MAX
  };

  using PrefetchDataMap =
      GlowplugKeyValueData<PrefetchData, internal::LastVisitTimeCompare>;
  using RedirectDataMap =
      GlowplugKeyValueData<RedirectData, internal::LastVisitTimeCompare>;
  using ManifestDataMap = GlowplugKeyValueData<precache::PrecacheManifest,
                                               internal::ManifestCompare>;
  using OriginDataMap =
      GlowplugKeyValueData<OriginData, internal::LastVisitTimeCompare>;
  using NavigationMap =
      std::map<NavigationID, std::unique_ptr<PageRequestSummary>>;

  ResourcePrefetchPredictor(const LoadingPredictorConfig& config,
                            Profile* profile);
  ~ResourcePrefetchPredictor() override;

  // Starts initialization by posting a task to the DB thread to read the
  // predictor database. Virtual for testing.
  virtual void StartInitialization();
  virtual void Shutdown();

  // Determines the resource type from the declared one, falling back to MIME
  // type detection when it is not explicit.
  static content::ResourceType GetResourceType(
      content::ResourceType resource_type,
      const std::string& mime_type);

  // Determines the ResourceType from the mime type, defaulting to the
  // |fallback| if the ResourceType could not be determined.
  static content::ResourceType GetResourceTypeFromMimeType(
      const std::string& mime_type,
      content::ResourceType fallback);

  // Called when ResourcePrefetcher is finished, i.e. there is nothing pending
  // in flight.
  void OnPrefetchingFinished(
      const GURL& main_frame_url,
      std::unique_ptr<ResourcePrefetcher::PrefetcherStats> stats);

  // Returns true if prefetching data exists for the |main_frame_url|.
  virtual bool IsUrlPrefetchable(const GURL& main_frame_url) const;

  // Returns true iff |resource| has sufficient confidence level and required
  // number of hits.
  bool IsResourcePrefetchable(const ResourceData& resource) const;

  // precache::PrecacheManager::Delegate:
  void OnManifestFetched(const std::string& host,
                         const precache::PrecacheManifest& manifest) override;

  // Sets the |observer| to be notified when the resource prefetch predictor
  // data changes. Previously registered observer will be discarded. Call
  // this with nullptr parameter to de-register observer.
  void SetObserverForTesting(TestObserver* observer);

  // Sets the |stats_collector| which is used to report histograms.
  void SetStatsCollector(LoadingStatsCollector* stats_collector);

  // Returns true iff there is PrefetchData that can be used for a
  // |main_frame_url| and fills |prediction| with resources that need to be
  // prefetched. |prediction| pointer may be nullptr to get return value only.
  virtual bool GetPrefetchData(const GURL& main_frame_url,
                               Prediction* prediction) const;

 private:
  // 'LoadingPredictorObserver' calls the below functions to inform the
  // predictor of main frame and resource requests. Should only be called if the
  // corresponding Should* functions return true.
  void RecordURLRequest(const URLRequestSummary& request);
  void RecordURLResponse(const URLRequestSummary& response);
  void RecordURLRedirect(const URLRequestSummary& response);

  // Called when the main frame of a page completes loading.
  void RecordMainFrameLoadComplete(const NavigationID& navigation_id);

  // Called after the main frame's first contentful paint.
  void RecordFirstContentfulPaint(
      const NavigationID& navigation_id,
      const base::TimeTicks& first_contentful_paint);

  // Starts prefetching for |main_frame_url| from a |prediction|.
  void StartPrefetching(const GURL& main_frame_url,
                        const Prediction& prediction);

  // Stops prefetching that may be in progress corresponding to
  // |main_frame_url|.
  void StopPrefetching(const GURL& main_frame_url);

  friend class LoadingPredictor;
  friend class ::PredictorsHandler;
  friend class LoadingDataCollector;
  friend class ResourcePrefetchPredictorTest;
  friend class ResourcePrefetchPredictorBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, DeleteUrls);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           LazilyInitializeEmpty);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           LazilyInitializeWithData);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           NavigationLowHistoryCount);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, NavigationUrlInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, NavigationUrlNotInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           NavigationUrlNotInDBAndDBFull);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, RedirectUrlNotInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, RedirectUrlInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, ManifestHostNotInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, ManifestHostInDB);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           ManifestHostNotInDBAndDBFull);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           ManifestUnusedRemoved);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, OnMainFrameRequest);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, OnMainFrameRedirect);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           OnSubresourceResponse);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, GetCorrectPLT);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           PopulatePrefetcherRequest);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, PopulateFromManifest);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, GetRedirectEndpoint);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest, GetPrefetchData);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           TestPrecisionRecallHistograms);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           TestPrefetchingDurationHistogram);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTest,
                           TestRecordFirstContentfulPaint);

  enum InitializationState {
    NOT_INITIALIZED = 0,
    INITIALIZING = 1,
    INITIALIZED = 2
  };
  // Returns true if the request (should have a response in it) is "no-store".
  static bool IsNoStore(const net::URLRequest& request);

  // Functions called on different network events pertaining to the loading of
  // main frame resource or sub resources.
  void OnMainFrameRequest(const URLRequestSummary& request);
  void OnMainFrameRedirect(const URLRequestSummary& response);
  void OnSubresourceResponse(const URLRequestSummary& response);
  void OnSubresourceRedirect(const URLRequestSummary& response);

  // Called when onload completes for a navigation. We treat this point as the
  // "completion" of the navigation. The resources requested by the page up to
  // this point are the only ones considered for prefetching.
  void OnNavigationComplete(const NavigationID& nav_id_without_timing_info);

  // Returns true iff one of the following conditions is true
  // * |redirect_data| contains confident redirect endpoint for |entry_point|
  //   and assigns it to the |redirect_endpoint|
  //
  // * |redirect_data| doens't contain an entry for |entry_point| and assigns
  //   |entry_point| to the |redirect_endpoint|.
  bool GetRedirectEndpoint(const std::string& entry_point,
                           const RedirectDataMap& redirect_data,
                           std::string* redirect_endpoint) const;

  // Returns true iff the |resource_data| contains PrefetchData that can be used
  // for a |main_frame_key| and fills |urls| with resources that need to be
  // prefetched. |urls| may be nullptr to get the return value only.
  bool PopulatePrefetcherRequest(const std::string& main_frame_key,
                                 const PrefetchDataMap& resource_data,
                                 std::vector<GURL>* urls) const;

  // Returns true iff the manifest table contains PrecacheManifest that can be
  // used for a |manifest_host| and fills |urls| with resources that need to be
  // prefetched. |urls| may be nullptr to get the return value only.
  bool PopulateFromManifest(const std::string& manifest_host,
                            std::vector<GURL>* urls) const;

  // Callback for the task to read the predictor database. Takes ownership of
  // all arguments.
  void CreateCaches(std::unique_ptr<PrefetchDataMap> url_resource_data,
                    std::unique_ptr<PrefetchDataMap> host_resource_data,
                    std::unique_ptr<RedirectDataMap> url_redirect_data,
                    std::unique_ptr<RedirectDataMap> host_redirect_data,
                    std::unique_ptr<ManifestDataMap> manifest_data,
                    std::unique_ptr<OriginDataMap> origin_data);

  // Called during initialization when history is read and the predictor
  // database has been read.
  void OnHistoryAndCacheLoaded();

  // Cleanup inflight_navigations_ and call a cleanup for stats_collector_.
  void CleanupAbandonedNavigations(const NavigationID& navigation_id);

  // Deletes all URLs from the predictor database, the caches and removes all
  // inflight navigations.
  void DeleteAllUrls();

  // Deletes data for the input |urls| and their corresponding hosts from the
  // predictor database and caches.
  void DeleteUrls(const history::URLRows& urls);

  // Callback for GetUrlVisitCountTask.
  void OnVisitCountLookup(size_t url_visit_count,
                          const PageRequestSummary& summary);

  // Merges resources in |new_resources| into the |resource_data| and
  // correspondingly updates the predictor database.
  void LearnNavigation(const std::string& key,
                       const std::vector<URLRequestSummary>& new_resources,
                       PrefetchDataMap* resource_data);

  // Updates information about final redirect destination for the |key| in
  // |redirect_data| and correspondingly updates the predictor database.
  void LearnRedirect(const std::string& key,
                     const std::string& final_redirect,
                     RedirectDataMap* redirect_data);

  void LearnOrigins(const std::string& host,
                    const std::map<GURL, OriginRequestSummary>& summaries);

  // Reports database readiness metric defined as percentage of navigated hosts
  // found in DB for last X entries in history.
  void ReportDatabaseReadiness(const history::TopHostsList& top_hosts) const;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;

  // Updates list of resources in the |resource_data| for the |key| according to
  // the |manifest|.
  void UpdatePrefetchDataByManifest(const std::string& key,
                                    PrefetchDataMap* resource_data,
                                    const precache::PrecacheManifest& manifest);

  // Used to connect to HistoryService or register for service loaded
  // notificatioan.
  void ConnectToHistoryService();

  // Used for testing to inject mock tables.
  void set_mock_tables(scoped_refptr<ResourcePrefetchPredictorTables> tables) {
    tables_ = tables;
  }

  // For testing.
  void set_mock_resource_prefetcher_manager(
      scoped_refptr<ResourcePrefetcherManager> prefetch_manager) {
    prefetch_manager_ = prefetch_manager;
  }

  Profile* const profile_;
  TestObserver* observer_;
  LoadingStatsCollector* stats_collector_;
  const LoadingPredictorConfig config_;
  InitializationState initialization_state_;
  scoped_refptr<ResourcePrefetchPredictorTables> tables_;
  scoped_refptr<ResourcePrefetcherManager> prefetch_manager_;
  base::CancelableTaskTracker history_lookup_consumer_;

  std::unique_ptr<PrefetchDataMap> url_resource_data_;
  std::unique_ptr<PrefetchDataMap> host_resource_data_;
  std::unique_ptr<RedirectDataMap> url_redirect_data_;
  std::unique_ptr<RedirectDataMap> host_redirect_data_;
  std::unique_ptr<ManifestDataMap> manifest_data_;
  std::unique_ptr<OriginDataMap> origin_data_;

  NavigationMap inflight_navigations_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  base::WeakPtrFactory<ResourcePrefetchPredictor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictor);
};

// An interface used to notify that data in the ResourcePrefetchPredictor
// has changed. All methods are invoked on the UI thread.
class TestObserver {
 public:
  // De-registers itself from |predictor_| on destruction.
  virtual ~TestObserver();

  virtual void OnPredictorInitialized() {}

  virtual void OnNavigationLearned(
      size_t url_visit_count,
      const ResourcePrefetchPredictor::PageRequestSummary& summary) {}

  virtual void OnPrefetchingStarted(const GURL& main_frame_url) {}

  virtual void OnPrefetchingStopped(const GURL& main_frame_url) {}

  virtual void OnPrefetchingFinished(const GURL& main_frame_url) {}

 protected:
  // |predictor| must be non-NULL and has to outlive the TestObserver.
  // Also the predictor must not have a TestObserver set.
  explicit TestObserver(ResourcePrefetchPredictor* predictor);

 private:
  ResourcePrefetchPredictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_H_

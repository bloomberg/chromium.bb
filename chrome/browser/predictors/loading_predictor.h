// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace predictors {

class ResourcePrefetchPredictor;
class LoadingStatsCollector;
class TestLoadingObserver;

// Entry point for the Loading predictor.
// From a high-level request (GURL and motivation) and a database of historical
// data, initiates predictive actions to speed up page loads.
//
// Also listens to main frame requests/redirects/responses to initiate and
// cancel page load hints.
//
// See ResourcePrefetchPredictor for a description of the resource prefetch
// predictor.
//
// All methods must be called from the UI thread.
class LoadingPredictor : public KeyedService,
                         public ResourcePrefetcher::Delegate,
                         public PreconnectManager::Delegate {
 public:
  LoadingPredictor(const LoadingPredictorConfig& config, Profile* profile);
  ~LoadingPredictor() override;

  // Hints that a page load is expected for |url|, with the hint coming from a
  // given |origin|. May trigger actions, such as prefetch and/or preconnect.
  void PrepareForPageLoad(const GURL& url, HintOrigin origin);

  // Indicates that a page load hint is no longer active.
  void CancelPageLoadHint(const GURL& url);

  // Starts initialization, will complete asynchronously.
  void StartInitialization();

  // Don't use, internal only.
  ResourcePrefetchPredictor* resource_prefetch_predictor();
  LoadingDataCollector* loading_data_collector();

  // KeyedService:
  void Shutdown() override;

  void OnMainFrameRequest(const URLRequestSummary& summary);
  void OnMainFrameRedirect(const URLRequestSummary& summary);
  void OnMainFrameResponse(const URLRequestSummary& summary);

  base::WeakPtr<LoadingPredictor> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // ResourcePrefetcher::Delegate:
  void ResourcePrefetcherFinished(
      ResourcePrefetcher* prefetcher,
      std::unique_ptr<ResourcePrefetcher::PrefetcherStats> stats) override;

  // PreconnectManager::Delegate:
  void PreconnectFinished(const GURL& url) override;

  // Sets the |observer| to be notified when prefetches start and
  // finish. A previously registered observer will be discarded. Call this with
  // a nullptr parameter to de-register the observer.
  void SetObserverForTesting(TestLoadingObserver* observer);

 private:
  // Cancels an active hint, from its iterator inside |active_hints_|. If the
  // iterator is .end(), does nothing. Returns the iterator after deletion of
  // the entry.
  std::map<GURL, base::TimeTicks>::iterator CancelActiveHint(
      std::map<GURL, base::TimeTicks>::iterator hint_it);
  void CleanupAbandonedHintsAndNavigations(const NavigationID& navigation_id);

  // May start a prefetch of |urls| for |url| with a given hint |origin|. A new
  // prefetch may not start if there is already one in flight, for instance.
  void MaybeAddPrefetch(const GURL& url,
                        const std::vector<GURL>& urls,
                        HintOrigin origin);
  // If a prefetch exists for |url|, stop it.
  void MaybeRemovePrefetch(const GURL& url);

  // May start a preconnect to |preconnect_origins| and preresolve of
  // |preresolve_hosts| for |url| with a given hint |origin|.
  void MaybeAddPreconnect(const GURL& url,
                          const std::vector<GURL>& preconnect_origins,
                          const std::vector<GURL>& preresolve_hosts,
                          HintOrigin origin);
  // If a preconnect exists for |url|, stop it.
  void MaybeRemovePreconnect(const GURL& url);

  // For testing.
  void set_mock_resource_prefetch_predictor(
      std::unique_ptr<ResourcePrefetchPredictor> predictor) {
    resource_prefetch_predictor_ = std::move(predictor);
  }

  LoadingPredictorConfig config_;
  Profile* profile_;
  std::unique_ptr<ResourcePrefetchPredictor> resource_prefetch_predictor_;
  std::unique_ptr<LoadingStatsCollector> stats_collector_;
  std::unique_ptr<LoadingDataCollector> loading_data_collector_;
  std::unique_ptr<PreconnectManager> preconnect_manager_;
  std::map<GURL, base::TimeTicks> active_hints_;
  // Initial URL.
  std::map<NavigationID, GURL> active_navigations_;
  // The value is {prefetcher, already deleted}.
  std::map<std::string, std::pair<std::unique_ptr<ResourcePrefetcher>, bool>>
      prefetches_;
  TestLoadingObserver* observer_;
  bool shutdown_ = false;

  friend class LoadingPredictorTest;
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameResponseCancelsHint);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameRequestCancelsStaleNavigations);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameResponseClearsNavigations);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameRequestDoesntCancelExternalHint);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestDontTrackNonPrefetchableUrls);

  base::WeakPtrFactory<LoadingPredictor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictor);
};

// An interface used to notify that data in the LoadingPredictor has
// changed. All methods are called on the UI thread.
class TestLoadingObserver {
 public:
  // De-registers itself from |predictor_| on destruction.
  virtual ~TestLoadingObserver();

  virtual void OnPrefetchingStarted(const GURL& main_frame_url) {}

  virtual void OnPrefetchingStopped(const GURL& main_frame_url) {}

  virtual void OnPrefetchingFinished(const GURL& main_frame_url) {}

 protected:
  // |predictor| must be non-NULL and has to outlive the LoadingTestObserver.
  // Also the predictor must not have a LoadingTestObserver set.
  explicit TestLoadingObserver(LoadingPredictor* predictor);

 private:
  LoadingPredictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(TestLoadingObserver);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_

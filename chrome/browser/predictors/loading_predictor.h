// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_

#include <map>
#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace predictors {

class ResourcePrefetchPredictor;
class LoadingStatsCollector;

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
class LoadingPredictor : public KeyedService {
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

  void OnMainFrameRequest(
      const ResourcePrefetchPredictor::URLRequestSummary& summary);
  void OnMainFrameRedirect(
      const ResourcePrefetchPredictor::URLRequestSummary& summary);
  void OnMainFrameResponse(
      const ResourcePrefetchPredictor::URLRequestSummary& summary);

  base::WeakPtr<LoadingPredictor> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Cancels an active hint, from its iterator inside |active_hints_|. If the
  // iterator is .end(), does nothing. Returns the iterator after deletion of
  // the entry.
  std::map<GURL, base::TimeTicks>::iterator CancelActiveHint(
      std::map<GURL, base::TimeTicks>::iterator hint_it);
  void CleanupAbandonedHintsAndNavigations(const NavigationID& navigation_id);

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
  std::map<GURL, base::TimeTicks> active_hints_;
  // Initial URL.
  std::map<NavigationID, GURL> active_navigations_;

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

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_

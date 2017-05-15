// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"

namespace predictors {

bool IsLoadingPredictortEnabled(Profile* profile,
                                LoadingPredictorConfig* config) {
  return IsSpeculativeResourcePrefetchingEnabled(profile, config);
}

LoadingPredictorConfig::LoadingPredictorConfig()
    : mode(0),
      max_navigation_lifetime_seconds(60),
      max_urls_to_track(500),
      max_hosts_to_track(200),
      min_url_visit_count(2),
      max_resources_per_entry(50),
      max_origins_per_entry(50),
      max_consecutive_misses(3),
      max_redirect_consecutive_misses(5),
      min_resource_confidence_to_trigger_prefetch(0.7f),
      min_resource_hits_to_trigger_prefetch(2),
      max_prefetches_inflight_per_navigation(5),
      max_prefetches_inflight_per_host_per_navigation(3),
      is_url_learning_enabled(false),
      is_manifests_enabled(false),
      is_origin_learning_enabled(false) {}

LoadingPredictorConfig::LoadingPredictorConfig(
    const LoadingPredictorConfig& other) = default;

LoadingPredictorConfig::~LoadingPredictorConfig() = default;

bool LoadingPredictorConfig::IsLearningEnabled() const {
  return (mode & LEARNING) > 0;
}

bool LoadingPredictorConfig::IsPrefetchingEnabledForSomeOrigin(
    Profile* profile) const {
  int mask = PREFETCHING_FOR_NAVIGATION | PREFETCHING_FOR_EXTERNAL;
  return internal::IsPrefetchingEnabledInternal(profile, mode, mask);
}

bool LoadingPredictorConfig::IsPrefetchingEnabledForOrigin(
    Profile* profile,
    HintOrigin origin) const {
  int mask = 0;
  switch (origin) {
    case HintOrigin::NAVIGATION:
      mask = PREFETCHING_FOR_NAVIGATION;
      break;
    case HintOrigin::EXTERNAL:
      mask = PREFETCHING_FOR_EXTERNAL;
      break;
  }
  return internal::IsPrefetchingEnabledInternal(profile, mode, mask);
}

bool LoadingPredictorConfig::IsLowConfidenceForTest() const {
  return min_url_visit_count == 1 &&
         std::abs(min_resource_confidence_to_trigger_prefetch - 0.5f) < 1e-6 &&
         min_resource_hits_to_trigger_prefetch == 1;
}

bool LoadingPredictorConfig::IsHighConfidenceForTest() const {
  return min_url_visit_count == 3 &&
         std::abs(min_resource_confidence_to_trigger_prefetch - 0.9f) < 1e-6 &&
         min_resource_hits_to_trigger_prefetch == 3;
}

bool LoadingPredictorConfig::IsMoreResourcesEnabledForTest() const {
  return max_resources_per_entry == 100;
}

bool LoadingPredictorConfig::IsSmallDBEnabledForTest() const {
  return max_urls_to_track == 200 && max_hosts_to_track == 100;
}

}  // namespace predictors

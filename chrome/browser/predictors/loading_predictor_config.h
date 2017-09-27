// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_CONFIG_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_CONFIG_H_

#include <cstddef>

#include "base/feature_list.h"

class Profile;

namespace predictors {

extern const char kSpeculativePreconnectFeatureName[];
extern const char kPreconnectMode[];
extern const char kNoPreconnectMode[];
extern const base::Feature kSpeculativePreconnectFeature;

struct LoadingPredictorConfig;

// Returns whether the predictor is enabled, and populates |config|, if not
// nullptr.
bool IsLoadingPredictorEnabled(Profile* profile,
                               LoadingPredictorConfig* config);

// Returns true if speculative preconnect is enabled, and initializes |config|,
// if not nullptr.
bool MaybeEnableSpeculativePreconnect(LoadingPredictorConfig* config);

// Returns true if all other implementations of preconnect should be disabled.
bool ShouldDisableOtherPreconnects();

// Indicates what caused the page load hint.
enum class HintOrigin { NAVIGATION, EXTERNAL, OMNIBOX };

// Represents the config for the Loading predictor.
struct LoadingPredictorConfig {
  // Initializes the config with default values.
  LoadingPredictorConfig();
  LoadingPredictorConfig(const LoadingPredictorConfig& other);
  ~LoadingPredictorConfig();

  // The mode the LoadingPredictor is running in. Forms a bit map.
  enum Mode {
    LEARNING = 1 << 0,
    PREFETCHING_FOR_NAVIGATION = 1 << 2,
    PREFETCHING_FOR_EXTERNAL = 1 << 3,
    PRECONNECT = 1 << 4
  };
  int mode;

  // Helpers to deal with mode.
  bool IsLearningEnabled() const;
  bool IsPrefetchingEnabledForSomeOrigin(Profile* profile) const;
  bool IsPrefetchingEnabledForOrigin(Profile* profile, HintOrigin origin) const;
  bool IsPreconnectEnabledForSomeOrigin(Profile* profile) const;
  bool IsPreconnectEnabledForOrigin(Profile* profile, HintOrigin origin) const;

  bool IsLowConfidenceForTest() const;
  bool IsHighConfidenceForTest() const;
  bool IsMoreResourcesEnabledForTest() const;
  bool IsSmallDBEnabledForTest() const;

  // If a navigation hasn't seen a load complete event in this much time, it
  // is considered abandoned.
  size_t max_navigation_lifetime_seconds;

  // Size of LRU caches for the URL and host data.
  size_t max_urls_to_track;
  size_t max_hosts_to_track;

  // The number of times we should have seen a visit to this URL in history
  // to start tracking it. This is to ensure we don't bother with oneoff
  // entries. For hosts we track each one.
  size_t min_url_visit_count;

  // The maximum number of resources to store per entry.
  size_t max_resources_per_entry;
  // The maximum number of origins to store per entry.
  size_t max_origins_per_entry;
  // The number of consecutive misses after which we stop tracking a resource
  // URL.
  size_t max_consecutive_misses;
  // The number of consecutive misses after which we stop tracking a redirect
  // endpoint.
  size_t max_redirect_consecutive_misses;

  // The minimum confidence (accuracy of hits) required for a resource to be
  // prefetched.
  float min_resource_confidence_to_trigger_prefetch;
  // The minimum number of times we must have a URL on record to prefetch it.
  size_t min_resource_hits_to_trigger_prefetch;

  // Maximum number of prefetches that can be inflight for a single navigation.
  size_t max_prefetches_inflight_per_navigation;
  // Maximum number of prefetches that can be inflight for a host for a single
  // navigation.
  size_t max_prefetches_inflight_per_host_per_navigation;
  // True iff the predictor can use a host-based subresources database.
  bool is_host_learning_enabled;
  // True iff the predictor can use a url-based subresources database.
  bool is_url_learning_enabled;
  // True iff origin-based learning is enabled.
  bool is_origin_learning_enabled;

  // True iff all other implementations of preconnect should be disabled.
  bool should_disable_other_preconnects;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_CONFIG_H_

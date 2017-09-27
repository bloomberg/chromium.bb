// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_config.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace predictors {

namespace {

bool IsPrefetchingEnabledInternal(Profile* profile, int mode, int mask) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if ((mode & mask) == 0)
    return false;

  if (!profile || !profile->GetPrefs() ||
      chrome_browser_net::CanPrefetchAndPrerenderUI(profile->GetPrefs()) !=
          chrome_browser_net::NetworkPredictionStatus::ENABLED) {
    return false;
  }

  return true;
}

bool IsPreconnectEnabledInternal(Profile* profile, int mode, int mask) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if ((mode & mask) == 0)
    return false;

  if (!profile || !profile->GetPrefs() ||
      !chrome_browser_net::CanPreresolveAndPreconnectUI(profile->GetPrefs())) {
    return false;
  }

  return true;
}

}  // namespace

const char kSpeculativePreconnectFeatureName[] = "SpeculativePreconnect";
const char kPreconnectMode[] = "preconnect";
const char kNoPreconnectMode[] = "no-preconnect";

const base::Feature kSpeculativePreconnectFeature{
    kSpeculativePreconnectFeatureName, base::FEATURE_DISABLED_BY_DEFAULT};

bool MaybeEnableSpeculativePreconnect(LoadingPredictorConfig* config) {
  if (!base::FeatureList::IsEnabled(kSpeculativePreconnectFeature))
    return false;

  std::string mode_value = base::GetFieldTrialParamValueByFeature(
      kSpeculativePreconnectFeature, kModeParamName);
  if (mode_value == kLearningMode) {
    if (config) {
      config->mode |= LoadingPredictorConfig::LEARNING;
      config->is_origin_learning_enabled = true;
    }
    return true;
  } else if (mode_value == kPreconnectMode) {
    if (config) {
      config->mode |=
          LoadingPredictorConfig::LEARNING | LoadingPredictorConfig::PRECONNECT;
      config->is_origin_learning_enabled = true;
      config->should_disable_other_preconnects = true;
    }
    return true;
  } else if (mode_value == kNoPreconnectMode) {
    if (config) {
      config->should_disable_other_preconnects = true;
    }
    return false;
  }

  return false;
}

bool ShouldDisableOtherPreconnects() {
  LoadingPredictorConfig config;
  MaybeEnableSpeculativePreconnect(&config);
  return config.should_disable_other_preconnects;
}

bool IsLoadingPredictorEnabled(Profile* profile,
                               LoadingPredictorConfig* config) {
  // Disabled for of-the-record. Policy choice, not a technical limitation.
  if (!profile || profile->IsOffTheRecord())
    return false;

  // Compute both statements because they have side effects.
  bool resource_prefetching_enabled = MaybeEnableResourcePrefetching(config);
  bool preconnect_enabled = MaybeEnableSpeculativePreconnect(config);
  return resource_prefetching_enabled || preconnect_enabled;
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
      is_host_learning_enabled(false),
      is_url_learning_enabled(false),
      is_origin_learning_enabled(false),
      should_disable_other_preconnects(false) {}

LoadingPredictorConfig::LoadingPredictorConfig(
    const LoadingPredictorConfig& other) = default;

LoadingPredictorConfig::~LoadingPredictorConfig() = default;

bool LoadingPredictorConfig::IsLearningEnabled() const {
  return (mode & LEARNING) > 0;
}

bool LoadingPredictorConfig::IsPrefetchingEnabledForSomeOrigin(
    Profile* profile) const {
  int mask = PREFETCHING_FOR_NAVIGATION | PREFETCHING_FOR_EXTERNAL;
  return IsPrefetchingEnabledInternal(profile, mode, mask);
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
    case HintOrigin::OMNIBOX:
      // No prefetching for omnibox.
      break;
  }
  return IsPrefetchingEnabledInternal(profile, mode, mask);
}

bool LoadingPredictorConfig::IsPreconnectEnabledForSomeOrigin(
    Profile* profile) const {
  return IsPreconnectEnabledInternal(profile, mode, PRECONNECT);
}

bool LoadingPredictorConfig::IsPreconnectEnabledForOrigin(
    Profile* profile,
    HintOrigin origin) const {
  return IsPreconnectEnabledInternal(profile, mode, PRECONNECT);
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

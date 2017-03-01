// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_common.h"

#include <string>
#include <tuple>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace predictors {

const char kSpeculativeResourcePrefetchingFeatureName[] =
    "SpeculativeResourcePrefetching";
const char kModeParamName[] = "mode";
const char kLearningMode[] = "learning";
const char kExternalPrefetchingMode[] = "external-prefetching";
const char kPrefetchingMode[] = "prefetching";
const char kEnableUrlLearningParamName[] = "enable-url-learning";

namespace {

const base::Feature kSpeculativeResourcePrefetchingFeature{
    kSpeculativeResourcePrefetchingFeatureName,
    base::FEATURE_DISABLED_BY_DEFAULT};

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

}  // namespace

bool IsSpeculativeResourcePrefetchingEnabled(
    Profile* profile,
    ResourcePrefetchPredictorConfig* config) {
  DCHECK(config);

  // Disabled for of-the-record. Policy choice, not a technical limitation.
  if (!profile || profile->IsOffTheRecord())
    return false;

  if (!base::FeatureList::IsEnabled(kSpeculativeResourcePrefetchingFeature))
    return false;

  std::string enable_url_learning_value =
      base::GetFieldTrialParamValueByFeature(
          kSpeculativeResourcePrefetchingFeature, kEnableUrlLearningParamName);
  if (enable_url_learning_value == "true") {
    config->is_url_learning_enabled = true;
  }

  std::string mode_value = base::GetFieldTrialParamValueByFeature(
      kSpeculativeResourcePrefetchingFeature, kModeParamName);
  if (mode_value == kLearningMode) {
    config->mode |= ResourcePrefetchPredictorConfig::LEARNING;
    return true;
  } else if (mode_value == kExternalPrefetchingMode) {
    config->mode |= ResourcePrefetchPredictorConfig::LEARNING |
                    ResourcePrefetchPredictorConfig::PREFETCHING_FOR_EXTERNAL;
    return true;
  } else if (mode_value == kPrefetchingMode) {
    config->mode |= ResourcePrefetchPredictorConfig::LEARNING |
                    ResourcePrefetchPredictorConfig::PREFETCHING_FOR_EXTERNAL |
                    ResourcePrefetchPredictorConfig::PREFETCHING_FOR_NAVIGATION;
    return true;
  }

  return false;
}

NavigationID::NavigationID() : tab_id(-1) {}

NavigationID::NavigationID(const NavigationID& other)
    : tab_id(other.tab_id),
      main_frame_url(other.main_frame_url),
      creation_time(other.creation_time) {}

NavigationID::NavigationID(content::WebContents* web_contents)
    : tab_id(SessionTabHelper::IdForTab(web_contents)),
      main_frame_url(web_contents->GetLastCommittedURL()),
      creation_time(base::TimeTicks::Now()) {}

NavigationID::NavigationID(content::WebContents* web_contents,
                           const GURL& main_frame_url,
                           const base::TimeTicks& creation_time)
    : tab_id(SessionTabHelper::IdForTab(web_contents)),
      main_frame_url(main_frame_url),
      creation_time(creation_time) {}

bool NavigationID::is_valid() const {
  return tab_id != -1 && !main_frame_url.is_empty();
}

bool NavigationID::operator<(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  return std::tie(tab_id, main_frame_url) <
         std::tie(rhs.tab_id, rhs.main_frame_url);
}

bool NavigationID::operator==(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  return tab_id == rhs.tab_id && main_frame_url == rhs.main_frame_url;
}

ResourcePrefetchPredictorConfig::ResourcePrefetchPredictorConfig()
    : mode(0),
      max_navigation_lifetime_seconds(60),
      max_urls_to_track(500),
      max_hosts_to_track(200),
      min_url_visit_count(2),
      max_resources_per_entry(50),
      max_consecutive_misses(3),
      min_resource_confidence_to_trigger_prefetch(0.7f),
      min_resource_hits_to_trigger_prefetch(2),
      max_prefetches_inflight_per_navigation(5),
      max_prefetches_inflight_per_host_per_navigation(3),
      is_url_learning_enabled(false) {
}

ResourcePrefetchPredictorConfig::ResourcePrefetchPredictorConfig(
    const ResourcePrefetchPredictorConfig& other) = default;

ResourcePrefetchPredictorConfig::~ResourcePrefetchPredictorConfig() {
}

bool ResourcePrefetchPredictorConfig::IsLearningEnabled() const {
  return (mode & LEARNING) > 0;
}

bool ResourcePrefetchPredictorConfig::IsPrefetchingEnabledForSomeOrigin(
    Profile* profile) const {
  int mask = PREFETCHING_FOR_NAVIGATION | PREFETCHING_FOR_EXTERNAL;
  return IsPrefetchingEnabledInternal(profile, mode, mask);
}

bool ResourcePrefetchPredictorConfig::IsPrefetchingEnabledForOrigin(
    Profile* profile,
    PrefetchOrigin origin) const {
  int mask = 0;
  switch (origin) {
    case PrefetchOrigin::NAVIGATION:
      mask = PREFETCHING_FOR_NAVIGATION;
      break;
    case PrefetchOrigin::EXTERNAL:
      mask = PREFETCHING_FOR_EXTERNAL;
      break;
  }
  return IsPrefetchingEnabledInternal(profile, mode, mask);
}

bool ResourcePrefetchPredictorConfig::IsLowConfidenceForTest() const {
  return min_url_visit_count == 1 &&
      std::abs(min_resource_confidence_to_trigger_prefetch - 0.5f) < 1e-6 &&
      min_resource_hits_to_trigger_prefetch == 1;
}

bool ResourcePrefetchPredictorConfig::IsHighConfidenceForTest() const {
  return min_url_visit_count == 3 &&
      std::abs(min_resource_confidence_to_trigger_prefetch - 0.9f) < 1e-6 &&
      min_resource_hits_to_trigger_prefetch == 3;
}

bool ResourcePrefetchPredictorConfig::IsMoreResourcesEnabledForTest() const {
  return max_resources_per_entry == 100;
}

bool ResourcePrefetchPredictorConfig::IsSmallDBEnabledForTest() const {
  return max_urls_to_track == 200 && max_hosts_to_track == 100;
}

}  // namespace predictors

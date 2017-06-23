// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_common.h"

#include <string>
#include <tuple>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
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
const char kEnableManifestsParamName[] = "enable-manifests";
const char kEnableOriginLearningParamName[] = "enable-origin-learning";

const base::Feature kSpeculativeResourcePrefetchingFeature{
    kSpeculativeResourcePrefetchingFeatureName,
    base::FEATURE_DISABLED_BY_DEFAULT};

namespace internal {

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

}  // namespace internal

bool IsSpeculativeResourcePrefetchingEnabled(Profile* profile,
                                             LoadingPredictorConfig* config) {
  // Disabled for of-the-record. Policy choice, not a technical limitation.
  if (!profile || profile->IsOffTheRecord())
    return false;

  if (!base::FeatureList::IsEnabled(kSpeculativeResourcePrefetchingFeature))
    return false;

  if (config) {
    std::string enable_url_learning_value =
        base::GetFieldTrialParamValueByFeature(
            kSpeculativeResourcePrefetchingFeature,
            kEnableUrlLearningParamName);
    if (enable_url_learning_value == "true")
      config->is_url_learning_enabled = true;

    std::string enable_manifests_value = base::GetFieldTrialParamValueByFeature(
        kSpeculativeResourcePrefetchingFeature, kEnableManifestsParamName);
    if (enable_manifests_value == "true")
      config->is_manifests_enabled = true;

    bool enable_origin_learning = base::GetFieldTrialParamValueByFeature(
                                      kSpeculativeResourcePrefetchingFeature,
                                      kEnableOriginLearningParamName) == "true";
    config->is_origin_learning_enabled = enable_origin_learning;

    // Ensure that a resource that was only seen once is never prefetched. This
    // prevents the easy mistake of trying to prefetch an ephemeral url.
    DCHECK_GT(config->min_resource_hits_to_trigger_prefetch, 1U);
    if (config->min_resource_hits_to_trigger_prefetch < 2)
      config->min_resource_hits_to_trigger_prefetch = 2;
  }

  std::string mode_value = base::GetFieldTrialParamValueByFeature(
      kSpeculativeResourcePrefetchingFeature, kModeParamName);
  if (mode_value == kLearningMode) {
    if (config)
      config->mode |= LoadingPredictorConfig::LEARNING;
    return true;
  } else if (mode_value == kExternalPrefetchingMode) {
    if (config) {
      config->mode |= LoadingPredictorConfig::LEARNING |
                      LoadingPredictorConfig::PREFETCHING_FOR_EXTERNAL;
    }
    return true;
  } else if (mode_value == kPrefetchingMode) {
    if (config) {
      config->mode |= LoadingPredictorConfig::LEARNING |
                      LoadingPredictorConfig::PREFETCHING_FOR_EXTERNAL |
                      LoadingPredictorConfig::PREFETCHING_FOR_NAVIGATION;
    }
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

}  // namespace predictors

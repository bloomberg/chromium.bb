// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/features.h"

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>
#include "base/feature_list.h"

#include "base/macros.h"
#include "base/values.h"
namespace safe_browsing {
// Please define any new SafeBrowsing related features in this file, and add
// them to the ExperimentalFeaturesList below to start displaying their status
// on the chrome://safe-browsing page.

// Allows an ad sample report to be created but not sent. Used to measure
// performance impact of report generation.
const base::Feature kAdSamplerCollectButDontSendFeature{
    "SafeBrowsingAdSamplerCollectButDontSend",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls various parameters related to occasionally collecting ad samples,
// for example to control how often collection should occur.
const base::Feature kAdSamplerTriggerFeature{"SafeBrowsingAdSamplerTrigger",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kGaiaPasswordReuseReporting{
    "SyncPasswordReuseEvent", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kGoogleBrandedPhishingWarning{
    "PasswordProtectionGoogleBrandedPhishingWarning",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kThreatDomDetailsTagAndAttributeFeature{
    "ThreatDomDetailsTagAttributes", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTriggerThrottlerDailyQuotaFeature{
    "SafeBrowsingTriggerThrottlerDailyQuota",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDispatchSafetyNetCheckOffThread{
    "DispatchSafetyNetCheckOffThread", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAppendRecentNavigationEvents{
    "AppendRecentNavigationEvents", base::FEATURE_DISABLED_BY_DEFAULT};

namespace {
// List of experimental features. Boolean value for each list member should be
// set to true if the experiment is currently running at a probability other
// than 1 or 0, or to false otherwise.
constexpr struct {
  const base::Feature* feature;
  // True if the feature is running at a probability other than 1 or 0.
  bool probabilistically_enabled;
} kExperimentalFeatures[]{
    {&kAdSamplerCollectButDontSendFeature, false},
    {&kAdSamplerTriggerFeature, false},
    {&kAppendRecentNavigationEvents, true},
    {&kGaiaPasswordReuseReporting, true},
    {&kGoogleBrandedPhishingWarning, true},
    {&kThreatDomDetailsTagAndAttributeFeature, false},
    {&kTriggerThrottlerDailyQuotaFeature, false},
    {&kDispatchSafetyNetCheckOffThread, false},
};

// Adds the name and the enabled/disabled status of a given feature.
void AddFeatureAndAvailability(const base::Feature* exp_feature,
                               base::ListValue* param_list) {
  param_list->GetList().push_back(base::Value(exp_feature->name));
  if (base::FeatureList::IsEnabled(*exp_feature)) {
    param_list->GetList().push_back(base::Value("Enabled"));
  } else {
    param_list->GetList().push_back(base::Value("Disabled"));
  }
}
}  // namespace

// Returns the list of the experimental features that are enabled or disabled,
// as part of currently running Safe Browsing experiments.
base::ListValue GetFeatureStatusList() {
  base::ListValue param_list;
  for (const auto& feature_status : kExperimentalFeatures) {
    if (feature_status.probabilistically_enabled)
      AddFeatureAndAvailability(feature_status.feature, &param_list);
  }
  return param_list;
}

}  // namespace safe_browsing

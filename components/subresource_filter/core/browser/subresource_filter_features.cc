// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

namespace subresource_filter {

const base::Feature kSafeBrowsingSubresourceFilter{
    "SubresourceFilter", base::FEATURE_DISABLED_BY_DEFAULT};

// Legacy name `activation_state` is used in variation parameters.
const char kActivationLevelParameterName[] = "activation_state";
const char kActivationLevelDryRun[] = "dryrun";
const char kActivationLevelEnabled[] = "enabled";
const char kActivationLevelDisabled[] = "disabled";

const char kActivationScopeParameterName[] = "activation_scope";
const char kActivationScopeAllSites[] = "all_sites";
const char kActivationScopeActivationList[] = "activation_list";
const char kActivationScopeNoSites[] = "no_sites";

const char kActivationListsParameterName[] = "activation_lists";
const char kActivationListSocialEngineeringAdsInterstitial[] =
    "social_engineering_ads_interstitial";
const char kActivationListPhishingInterstitial[] = "phishing_interstitial";

const char kRulesetFlavorParameterName[] = "ruleset_flavor";

const char kPerformanceMeasurementRateParameterName[] =
    "performance_measurement_rate";

const char kSuppressNotificationsParameterName[] = "suppress_notifications";

const char kWhitelistSiteOnReloadParameterName[] = "whitelist_site_on_reload";

ActivationLevel GetMaximumActivationLevel() {
  std::string activation_level = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kActivationLevelParameterName);
  if (base::LowerCaseEqualsASCII(activation_level, kActivationLevelEnabled))
    return ActivationLevel::ENABLED;
  else if (base::LowerCaseEqualsASCII(activation_level, kActivationLevelDryRun))
    return ActivationLevel::DRYRUN;
  return ActivationLevel::DISABLED;
}

ActivationScope GetCurrentActivationScope() {
  std::string activation_scope = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kActivationScopeParameterName);
  if (base::LowerCaseEqualsASCII(activation_scope, kActivationScopeAllSites))
    return ActivationScope::ALL_SITES;
  else if (base::LowerCaseEqualsASCII(activation_scope,
                                      kActivationScopeActivationList))
    return ActivationScope::ACTIVATION_LIST;
  return ActivationScope::NO_SITES;
}

ActivationList GetCurrentActivationList() {
  std::string activation_lists = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kActivationListsParameterName);
  ActivationList activation_list_type = ActivationList::NONE;
  for (const base::StringPiece& activation_list :
       base::SplitStringPiece(activation_lists, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (base::LowerCaseEqualsASCII(activation_list,
                                   kActivationListPhishingInterstitial)) {
      return ActivationList::PHISHING_INTERSTITIAL;
    } else if (base::LowerCaseEqualsASCII(
                   activation_list,
                   kActivationListSocialEngineeringAdsInterstitial)) {
      activation_list_type = ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL;
    }
  }
  return activation_list_type;
}

double GetPerformanceMeasurementRate() {
  const std::string rate = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kPerformanceMeasurementRateParameterName);
  double value = 0;
  if (!base::StringToDouble(rate, &value) || value < 0)
    return 0;
  return value < 1 ? value : 1;
}

bool ShouldSuppressNotifications() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSafeBrowsingSubresourceFilter, kSuppressNotificationsParameterName,
      false /* default value */);
}

std::string GetRulesetFlavor() {
  return variations::GetVariationParamValueByFeature(
      subresource_filter::kSafeBrowsingSubresourceFilter,
      subresource_filter::kRulesetFlavorParameterName);
}

bool ShouldWhitelistSiteOnReload() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kSafeBrowsingSubresourceFilter, kWhitelistSiteOnReloadParameterName,
      false /* default value */);
}

}  // namespace subresource_filter

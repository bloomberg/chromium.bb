// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_

#include "base/feature_list.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_scope.h"

namespace subresource_filter {

// Encapsulates all parameters that define how the subresource filter feature
// should operate.
struct Configuration {
  Configuration();
  ~Configuration();
  Configuration(Configuration&&);
  Configuration& operator=(Configuration&&);

  // The maximum degree to which subresource filtering should be activated on
  // any RenderFrame. This will be ActivationLevel::DISABLED unless the feature
  // is enabled and variation parameters prescribe a higher activation level.
  ActivationLevel activation_level = ActivationLevel::DISABLED;

  // The activation scope. That is, the subset of page loads where subresource
  // filtering should be activated. This will be ActivationScope::NO_SITES
  // unless the feature is =enabled and variation parameters prescribe a wider
  // activation scope.
  ActivationScope activation_scope = ActivationScope::NO_SITES;

  // The activation list to use when the |activation_scope| is ACTIVATION_LIST.
  // This will be ActivationList::NONE unless variation parameters prescribe a
  // recognized list.
  ActivationList activation_list = ActivationList::NONE;

  // A number in the range [0, 1], indicating the fraction of page loads that
  // should have extended performance measurements enabled. The rate will
  // be 0 unless a greater frequency is specified by variation parameters.
  double performance_measurement_rate = 0.0;

  // Whether notifications indicating that a subresource was disallowed should
  // be suppressed in the UI.
  bool should_suppress_notifications = false;

  // The ruleset flavor to download through the component updater, or the empty
  // string if the default ruleset should be used.
  std::string ruleset_flavor;

  // Whether to whitelist a site when a page loaded from that site is reloaded.
  bool should_whitelist_site_on_reload = false;
};

// Retrieves the subresource filtering configuration to use. Expensive to call.
Configuration GetActiveConfiguration();

// Feature and variation parameter definitions -------------------------------

// The master toggle to enable/disable the Safe Browsing Subresource Filter.
extern const base::Feature kSafeBrowsingSubresourceFilter;

// Enables the new experimental UI for the Subresource Filter.
extern const base::Feature kSafeBrowsingSubresourceFilterExperimentalUI;

// Name/values of the variation parameter controlling maximum activation level.
extern const char kActivationLevelParameterName[];
extern const char kActivationLevelDryRun[];
extern const char kActivationLevelEnabled[];
extern const char kActivationLevelDisabled[];

extern const char kActivationScopeParameterName[];
extern const char kActivationScopeAllSites[];
extern const char kActivationScopeActivationList[];
extern const char kActivationScopeNoSites[];

extern const char kActivationListsParameterName[];
extern const char kActivationListSocialEngineeringAdsInterstitial[];
extern const char kActivationListPhishingInterstitial[];
extern const char kActivationListSubresourceFilter[];

extern const char kRulesetFlavorParameterName[];

extern const char kPerformanceMeasurementRateParameterName[];

extern const char kSuppressNotificationsParameterName[];

extern const char kWhitelistSiteOnReloadParameterName[];

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_

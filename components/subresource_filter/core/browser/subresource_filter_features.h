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

// The master toggle to enable/disable the Safe Browsing Subresource Filter.
extern const base::Feature kSafeBrowsingSubresourceFilter;

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

extern const char kRulesetFlavorParameterName[];

extern const char kPerformanceMeasurementRateParameterName[];

extern const char kSuppressNotificationsParameterName[];

extern const char kWhitelistSiteOnReloadParameterName[];

// Returns the maximum degree to which subresource filtering should be activated
// on any RenderFrame. This will be ActivationLevel::DISABLED unless the feature
// is enabled and variation parameters prescribe a higher activation level.
ActivationLevel GetMaximumActivationLevel();

// Returns the current activation scope, that is, the subset of page loads where
// subresource filtering should be activated. The function returns
// ActivationScope::NO_SITES unless the feature is enabled and variation
// parameters prescribe a wider activation scope.
ActivationScope GetCurrentActivationScope();

// Returns current activation list, based on the values from variation params in
// the feature |kSafeBrowsingSubresourceFilter|. When the corresponding
// variation param is empty, returns most conservative ActivationList::NONE.
ActivationList GetCurrentActivationList();

// Returns a number in the range [0, 1], indicating the fraction of page loads
// that should have extended performance measurements enabled. The rate will be
// 0 unless a greater frequency is specified by variation parameters.
double GetPerformanceMeasurementRate();

// Returns whether notifications indicating that a subresource was disallowed
// should be suppressed in the UI.
bool ShouldSuppressNotifications();

// Returns the ruleset flavor, or the empty string if the default ruleset should
// be used.
std::string GetRulesetFlavor();

// Returns whether the site of reloaded pages should be whitelisted.
bool ShouldWhitelistSiteOnReload();

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"

namespace subresource_filter {
namespace testing {

// Helper to override the state of the |kSafeBrowsingSubresourceFilter| feature,
// and its variation parameters, e.g., maximum activation level and activation
// scope. Expects a pre-existing global base::FieldTrialList singleton.
class ScopedSubresourceFilterFeatureToggle {
 public:
  ScopedSubresourceFilterFeatureToggle(
      base::FeatureList::OverrideState feature_state,
      const std::string& maximum_activation_level,
      const std::string& activation_scope,
      const std::string& activation_lists = std::string(),
      const std::string& performance_measurement_rate = std::string(),
      const std::string& suppress_notifications = std::string(),
      const std::string& whitelist_site_on_reload = std::string());

  ScopedSubresourceFilterFeatureToggle(
      base::FeatureList::OverrideState feature_state,
      std::map<std::string, std::string> variation_params);

  ~ScopedSubresourceFilterFeatureToggle();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSubresourceFilterFeatureToggle);
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_

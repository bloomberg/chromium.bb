// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"

#include <map>
#include <memory>

#include "base/metrics/field_trial.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {
namespace testing {

namespace {
constexpr const char kTestFieldTrialName[] = "FieldTrialNameShouldNotMatter";
constexpr const char kTestExperimentGroupName[] = "GroupNameShouldNotMatter";
}  // namespace

ScopedSubresourceFilterFeatureToggle::ScopedSubresourceFilterFeatureToggle(
    base::FeatureList::OverrideState feature_state,
    const std::string& maximum_activation_level,
    const std::string& activation_scope,
    const std::string& activation_lists,
    const std::string& performance_measurement_rate,
    const std::string& suppress_notifications,
    const std::string& whitelist_site_on_reload)
    : ScopedSubresourceFilterFeatureToggle(
          feature_state,
          {{kActivationLevelParameterName, maximum_activation_level},
           {kActivationScopeParameterName, activation_scope},
           {kActivationListsParameterName, activation_lists},
           {kPerformanceMeasurementRateParameterName,
            performance_measurement_rate},
           {kSuppressNotificationsParameterName, suppress_notifications},
           {kWhitelistSiteOnReloadParameterName, whitelist_site_on_reload}}) {}

ScopedSubresourceFilterFeatureToggle::ScopedSubresourceFilterFeatureToggle(
    base::FeatureList::OverrideState feature_state,
    std::map<std::string, std::string> variation_params) {
  variations::testing::ClearAllVariationParams();

  EXPECT_TRUE(variations::AssociateVariationParams(
      kTestFieldTrialName, kTestExperimentGroupName, variation_params));

  base::FieldTrial* field_trial = base::FieldTrialList::CreateFieldTrial(
      kTestFieldTrialName, kTestExperimentGroupName);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(kSafeBrowsingSubresourceFilter.name,
                                           feature_state, field_trial);

  // Since we are adding a scoped feature list after browser start, copy over
  // the existing feature list to prevent inconsistency.
  base::FeatureList* existing_feature_list = base::FeatureList::GetInstance();
  if (existing_feature_list) {
    std::string enabled_features;
    std::string disabled_features;
    base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                          &disabled_features);
    feature_list->InitializeFromCommandLine(enabled_features,
                                            disabled_features);
  }

  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
}

ScopedSubresourceFilterFeatureToggle::~ScopedSubresourceFilterFeatureToggle() {
  variations::testing::ClearAllVariationParams();
}

}  // namespace testing
}  // namespace subresource_filter

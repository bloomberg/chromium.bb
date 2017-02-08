// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TEST(SubresourceFilterFeaturesTest, ActivationLevel) {
  const struct {
    bool feature_enabled;
    const char* activation_level_param;
    ActivationLevel expected_activation_level;
  } kTestCases[] = {{false, "", ActivationLevel::DISABLED},
                    {false, "disabled", ActivationLevel::DISABLED},
                    {false, "dryrun", ActivationLevel::DISABLED},
                    {false, "enabled", ActivationLevel::DISABLED},
                    {false, "%$ garbage !%", ActivationLevel::DISABLED},
                    {true, "", ActivationLevel::DISABLED},
                    {true, "disable", ActivationLevel::DISABLED},
                    {true, "Disable", ActivationLevel::DISABLED},
                    {true, "disabled", ActivationLevel::DISABLED},
                    {true, "%$ garbage !%", ActivationLevel::DISABLED},
                    {true, kActivationLevelDryRun, ActivationLevel::DRYRUN},
                    {true, kActivationLevelEnabled, ActivationLevel::ENABLED},
                    {true, "Enabled", ActivationLevel::ENABLED}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationLevelParam = \"")
                 << test_case.activation_level_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        test_case.activation_level_param, kActivationScopeNoSites);

    EXPECT_EQ(test_case.expected_activation_level, GetMaximumActivationLevel());
    EXPECT_EQ(ActivationScope::NO_SITES, GetCurrentActivationScope());
  }
}

TEST(SubresourceFilterFeaturesTest, ActivationScope) {
  const struct {
    bool feature_enabled;
    const char* activation_scope_param;
    ActivationScope expected_activation_scope;
  } kTestCases[] = {
      {false, "", ActivationScope::NO_SITES},
      {false, "no_sites", ActivationScope::NO_SITES},
      {false, "allsites", ActivationScope::NO_SITES},
      {false, "enabled", ActivationScope::NO_SITES},
      {false, "%$ garbage !%", ActivationScope::NO_SITES},
      {true, "", ActivationScope::NO_SITES},
      {true, "nosites", ActivationScope::NO_SITES},
      {true, "No_sites", ActivationScope::NO_SITES},
      {true, "no_sites", ActivationScope::NO_SITES},
      {true, "%$ garbage !%", ActivationScope::NO_SITES},
      {true, kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationScopeParam = \"")
                 << test_case.activation_scope_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        kActivationLevelDisabled, test_case.activation_scope_param);

    EXPECT_EQ(ActivationLevel::DISABLED, GetMaximumActivationLevel());
    EXPECT_EQ(test_case.expected_activation_scope, GetCurrentActivationScope());
  }
}

TEST(SubresourceFilterFeaturesTest, ActivationLevelAndScope) {
  const struct {
    bool feature_enabled;
    const char* activation_level_param;
    ActivationLevel expected_activation_level;
    const char* activation_scope_param;
    ActivationScope expected_activation_scope;
  } kTestCases[] = {
      {false, kActivationLevelDisabled, ActivationLevel::DISABLED,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelDisabled, ActivationLevel::DISABLED,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelDisabled, ActivationLevel::DISABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelDisabled, ActivationLevel::DISABLED,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationLevelDisabled, ActivationLevel::DISABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelDryRun, ActivationLevel::DRYRUN,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelDryRun, ActivationLevel::DRYRUN,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelDryRun, ActivationLevel::DRYRUN,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationLevelDryRun, ActivationLevel::DRYRUN,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelEnabled, ActivationLevel::ENABLED,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationLevelEnabled, ActivationLevel::ENABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationLevelEnabled, ActivationLevel::ENABLED,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationLevelEnabled, ActivationLevel::ENABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {false, kActivationLevelEnabled, ActivationLevel::DISABLED,
       kActivationScopeAllSites, ActivationScope::NO_SITES}};

  for (const auto& test_case : kTestCases) {
    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        test_case.activation_level_param, test_case.activation_scope_param);

    EXPECT_EQ(test_case.expected_activation_scope, GetCurrentActivationScope());
    EXPECT_EQ(test_case.expected_activation_level, GetMaximumActivationLevel());
  }
}

TEST(SubresourceFilterFeaturesTest, ActivationList) {
  const std::string activation_soc_eng(
      kActivationListSocialEngineeringAdsInterstitial);
  const std::string activation_phishing(kActivationListPhishingInterstitial);
  const std::string socEngPhising = activation_soc_eng + activation_phishing;
  const std::string socEngCommaPhising =
      activation_soc_eng + "," + activation_phishing;
  const std::string phishingCommaSocEng =
      activation_phishing + "," + activation_soc_eng;
  const std::string socEngCommaPhisingCommaGarbage =
      socEngCommaPhising + "," + "Garbage";
  const struct {
    bool feature_enabled;
    const char* activation_list_param;
    ActivationList expected_activation_list;
  } kTestCases[] = {
      {false, "", ActivationList::NONE},
      {false, "social eng ads intertitial", ActivationList::NONE},
      {false, "phishing,interstital", ActivationList::NONE},
      {false, "%$ garbage !%", ActivationList::NONE},
      {true, "", ActivationList::NONE},
      {true, "social eng ads intertitial", ActivationList::NONE},
      {true, "phishing interstital", ActivationList::NONE},
      {true, "%$ garbage !%", ActivationList::NONE},
      {true, kActivationListSocialEngineeringAdsInterstitial,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL},
      {true, kActivationListPhishingInterstitial,
       ActivationList::PHISHING_INTERSTITIAL},
      {true, socEngPhising.c_str(), ActivationList::NONE},
      {true, socEngCommaPhising.c_str(), ActivationList::PHISHING_INTERSTITIAL},
      {true, phishingCommaSocEng.c_str(),
       ActivationList::PHISHING_INTERSTITIAL},
      {true, socEngCommaPhisingCommaGarbage.c_str(),
       ActivationList::PHISHING_INTERSTITIAL},
      {true, "List1, List2", ActivationList::NONE}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationListParam = \"")
                 << test_case.activation_list_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        kActivationLevelDisabled, kActivationScopeNoSites,
        test_case.activation_list_param);

    EXPECT_EQ(test_case.expected_activation_list, GetCurrentActivationList());
  }
}

TEST(SubresourceFilterFeaturesTest, PerfMeasurementRate) {
  const struct {
    bool feature_enabled;
    const char* perf_measurement_param;
    double expected_perf_measurement_rate;
  } kTestCases[] = {{false, "not_a_number", 0},
                    {false, "0", 0},
                    {false, "1", 0},
                    {true, "not_a_number", 0},
                    {true, "0.5not_a_number", 0},
                    {true, "0", 0},
                    {true, "0.000", 0},
                    {true, "0.05", 0.05},
                    {true, "0.5", 0.5},
                    {true, "1", 1},
                    {true, "1.0", 1},
                    {true, "0.333", 0.333},
                    {true, "1e0", 1}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("PerfMeasurementParam = \"")
                 << test_case.perf_measurement_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kPerformanceMeasurementRateParameterName,
          test_case.perf_measurement_param}});

    EXPECT_EQ(test_case.expected_perf_measurement_rate,
              GetPerformanceMeasurementRate());
  }
}

TEST(SubresourceFilterFeaturesTest, SuppressNotifications) {
  const struct {
    bool feature_enabled;
    const char* suppress_notifications_param;
    bool expected_suppress_notifications_value;
  } kTestCases[] = {{false, "", false},
                    {false, "true", false},
                    {false, "false", false},
                    {false, "invalid value", false},
                    {true, "", false},
                    {true, "false", false},
                    {true, "invalid value", false},
                    {true, "True", false},
                    {true, "TRUE", false},
                    {true, "true", true}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("SuppressNotificationsParam = \"")
                 << test_case.suppress_notifications_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kSuppressNotificationsParameterName,
          test_case.suppress_notifications_param}});

    EXPECT_EQ(test_case.expected_suppress_notifications_value,
              ShouldSuppressNotifications());
  }
}

TEST(SubresourceFilterFeaturesTest, WhitelistSiteOnReload) {
  const struct {
    bool feature_enabled;
    const char* whitelist_site_on_reload_param;
    bool expected_whitelist_site_on_reload_value;
  } kTestCases[] = {{false, "", false},
                    {false, "true", false},
                    {false, "false", false},
                    {false, "invalid value", false},
                    {true, "", false},
                    {true, "false", false},
                    {true, "invalid value", false},
                    {true, "True", false},
                    {true, "TRUE", false},
                    {true, "true", true}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("WhitelistSiteOnReloadParam = \"")
                 << test_case.whitelist_site_on_reload_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kWhitelistSiteOnReloadParameterName,
          test_case.whitelist_site_on_reload_param}});

    EXPECT_EQ(test_case.expected_whitelist_site_on_reload_value,
              ShouldWhitelistSiteOnReload());
  }
}

}  // namespace subresource_filter

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <map>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {
namespace testing {

namespace {

constexpr const char kTestFieldTrialName[] = "FieldTrialNameShouldNotMatter";
constexpr const char kTestExperimentGroupName[] = "GroupNameShouldNotMatter";

class ScopedExperimentalStateToggle {
 public:
  ScopedExperimentalStateToggle(
      base::FeatureList::OverrideState feature_state,
      std::map<std::string, std::string> variation_params)
      : field_trial_list_(nullptr /* entropy_provider */),
        scoped_configurator_(nullptr) {
    EXPECT_TRUE(base::AssociateFieldTrialParams(
        kTestFieldTrialName, kTestExperimentGroupName, variation_params));
    base::FieldTrial* field_trial = base::FieldTrialList::CreateFieldTrial(
        kTestFieldTrialName, kTestExperimentGroupName);

    std::unique_ptr<base::FeatureList> feature_list =
        base::MakeUnique<base::FeatureList>();
    feature_list->RegisterFieldTrialOverride(
        kSafeBrowsingSubresourceFilter.name, feature_state, field_trial);
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  ~ScopedExperimentalStateToggle() {
    variations::testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;

  ScopedSubresourceFilterConfigurator scoped_configurator_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExperimentalStateToggle);
};

}  // namespace
}  // namespace testing

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

    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, test_case.activation_level_param},
         {kActivationScopeParameterName, kActivationScopeNoSites}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(test_case.expected_activation_level,
              actual_configuration.activation_level);
    EXPECT_EQ(ActivationScope::NO_SITES, actual_configuration.activation_scope);
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

    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, kActivationLevelDisabled},
         {kActivationScopeParameterName, test_case.activation_scope_param}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(ActivationLevel::DISABLED, actual_configuration.activation_level);
    EXPECT_EQ(test_case.expected_activation_scope,
              actual_configuration.activation_scope);
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
    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, test_case.activation_level_param},
         {kActivationScopeParameterName, test_case.activation_scope_param}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(test_case.expected_activation_level,
              actual_configuration.activation_level);
    EXPECT_EQ(test_case.expected_activation_scope,
              actual_configuration.activation_scope);
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

    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, kActivationLevelDisabled},
         {kActivationScopeParameterName, kActivationScopeNoSites},
         {kActivationListsParameterName, test_case.activation_list_param}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(test_case.expected_activation_list,
              actual_configuration.activation_list);
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

    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kPerformanceMeasurementRateParameterName,
          test_case.perf_measurement_param}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(test_case.expected_perf_measurement_rate,
              actual_configuration.performance_measurement_rate);
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
                    {true, "True", true},
                    {true, "TRUE", true},
                    {true, "true", true}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("SuppressNotificationsParam = \"")
                 << test_case.suppress_notifications_param << "\"");

    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kSuppressNotificationsParameterName,
          test_case.suppress_notifications_param}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(test_case.expected_suppress_notifications_value,
              actual_configuration.should_suppress_notifications);
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
                    {true, "True", true},
                    {true, "TRUE", true},
                    {true, "true", true}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("WhitelistSiteOnReloadParam = \"")
                 << test_case.whitelist_site_on_reload_param << "\"");

    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kWhitelistSiteOnReloadParameterName,
          test_case.whitelist_site_on_reload_param}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(test_case.expected_whitelist_site_on_reload_value,
              actual_configuration.should_whitelist_site_on_reload);
  }
}

TEST(SubresourceFilterFeaturesTest, RulesetFlavor) {
  const struct {
    bool feature_enabled;
    const char* ruleset_flavor_param;
    const char* expected_ruleset_flavor_value;
  } kTestCases[] = {
      {false, "", ""}, {false, "a", ""}, {false, "test value", ""},
      {true, "", ""},  {true, "a", "a"}, {true, "test value", "test value"}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("Flavor = \"")
                 << test_case.ruleset_flavor_param << "\"");

    testing::ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kRulesetFlavorParameterName, test_case.ruleset_flavor_param}});

    const auto active_configurations = GetActiveConfigurations();
    const Configuration& actual_configuration =
        active_configurations->the_one_and_only();
    EXPECT_EQ(std::string(test_case.expected_ruleset_flavor_value),
              actual_configuration.ruleset_flavor);
  }
}
}  // namespace subresource_filter

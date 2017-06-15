// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

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

  testing::ScopedSubresourceFilterConfigurator scoped_configurator_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExperimentalStateToggle);
};

void ExpectAndRetrieveExactlyOneEnabledConfig(Configuration* actual_config) {
  DCHECK(actual_config);
  const auto config_list = GetEnabledConfigurations();
  ASSERT_EQ(1u, config_list->configs_by_decreasing_priority().size());
  *actual_config = config_list->configs_by_decreasing_priority().front();
}

void ExpectPresetCanBeEnabledByName(Configuration preset, const char* name) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      {{kEnablePresetsParameterName, name}});

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(config_list->configs_by_decreasing_priority(),
              ::testing::ElementsAre(preset, Configuration()));
}

void ExpectPresetIsEquivalentToVariationParams(
    Configuration preset,
    std::map<std::string, std::string> variation_params) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, variation_params);

  Configuration experimental_configuration;
  ExpectAndRetrieveExactlyOneEnabledConfig(&experimental_configuration);
  EXPECT_EQ(preset, experimental_configuration);
}

}  // namespace

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

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, test_case.activation_level_param},
         {kActivationScopeParameterName, kActivationScopeNoSites}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(test_case.expected_activation_level,
              actual_configuration.activation_options.activation_level);
    EXPECT_EQ(ActivationScope::NO_SITES,
              actual_configuration.activation_conditions.activation_scope);
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

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, kActivationLevelDisabled},
         {kActivationScopeParameterName, test_case.activation_scope_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(ActivationLevel::DISABLED,
              actual_configuration.activation_options.activation_level);
    EXPECT_EQ(test_case.expected_activation_scope,
              actual_configuration.activation_conditions.activation_scope);
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
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationLevelParam = \"")
                 << test_case.activation_level_param << "\"");
    SCOPED_TRACE(::testing::Message("ActivationScopeParam = \"")
                 << test_case.activation_scope_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, test_case.activation_level_param},
         {kActivationScopeParameterName, test_case.activation_scope_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(test_case.expected_activation_level,
              actual_configuration.activation_options.activation_level);
    EXPECT_EQ(test_case.expected_activation_scope,
              actual_configuration.activation_conditions.activation_scope);
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
      {false, "phishing,interstitial", ActivationList::NONE},
      {false, "%$ garbage !%", ActivationList::NONE},
      {true, "", ActivationList::NONE},
      {true, "social eng ads intertitial", ActivationList::NONE},
      {true, "phishing interstitial", ActivationList::NONE},
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

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationLevelParameterName, kActivationLevelDisabled},
         {kActivationScopeParameterName, kActivationScopeNoSites},
         {kActivationListsParameterName, test_case.activation_list_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(test_case.expected_activation_list,
              actual_configuration.activation_conditions.activation_list);
  }
}

TEST(SubresourceFilterFeaturesTest, ActivationPriority) {
  const struct {
    bool feature_enabled;
    const char* activation_priority_param;
    int expected_priority;
  } kTestCases[] = {{false, "", 0},
                    {false, "not_an_integer", 0},
                    {false, "100", 0},
                    {true, "", 0},
                    {true, "not_an_integer", 0},
                    {true, "0.5not_an_integer", 0},
                    {true, "garbage42", 0},
                    {true, "42garbage", 42},
                    {true, "0", 0},
                    {true, "1", 1},
                    {true, "-1", -1},
                    {true, "2.9", 2},
                    {true, "-2.9", -2},
                    {true, "2e0", 2},
                    {true, "100", 100}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("Priority = \"")
                 << test_case.activation_priority_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kActivationPriorityParameterName,
          test_case.activation_priority_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(test_case.expected_priority,
              actual_configuration.activation_conditions.priority);
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

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kPerformanceMeasurementRateParameterName,
          test_case.perf_measurement_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(
        test_case.expected_perf_measurement_rate,
        actual_configuration.activation_options.performance_measurement_rate);
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

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kSuppressNotificationsParameterName,
          test_case.suppress_notifications_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(
        test_case.expected_suppress_notifications_value,
        actual_configuration.activation_options.should_suppress_notifications);
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

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kWhitelistSiteOnReloadParameterName,
          test_case.whitelist_site_on_reload_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(test_case.expected_whitelist_site_on_reload_value,
              actual_configuration.activation_options
                  .should_whitelist_site_on_reload);
  }
}

TEST(SubresourceFilterFeaturesTest, StrengthenPopupBlocker) {
  const struct {
    bool feature_enabled;
    const char* strengthen_popup_blocker_param;
    bool expected_strengthen_popup_blocker_value;
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
    SCOPED_TRACE(::testing::Message("StrengthenPopupBlockerParam = \"")
                 << test_case.strengthen_popup_blocker_param << "\"");

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kStrengthenPopupBlockerParameterName,
          test_case.strengthen_popup_blocker_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(test_case.expected_strengthen_popup_blocker_value,
              actual_configuration.activation_options
                  .should_strengthen_popup_blocker);
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

    ScopedExperimentalStateToggle scoped_experimental_state(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        {{kRulesetFlavorParameterName, test_case.ruleset_flavor_param}});

    Configuration actual_configuration;
    ExpectAndRetrieveExactlyOneEnabledConfig(&actual_configuration);
    EXPECT_EQ(std::string(test_case.expected_ruleset_flavor_value),
              actual_configuration.general_settings.ruleset_flavor);
  }
}

TEST(SubresourceFilterFeaturesTest, LexicographicallyGreatestRulesetFlavor) {
  const struct {
    const char* expected_ruleset_flavor_selected;
    std::vector<std::string> ruleset_flavors;
  } kTestCases[] = {{"", std::vector<std::string>()},
                    {"", {""}},
                    {"a", {"a"}},
                    {"e", {"e"}},
                    {"foo", {"foo"}},
                    {"", {"", ""}},
                    {"a", {"a", ""}},
                    {"a", {"", "a"}},
                    {"a", {"a", "a"}},
                    {"c", {"b", "", "c"}},
                    {"b", {"", "b", "a"}},
                    {"aa", {"", "a", "aa"}},
                    {"b", {"", "a", "aa", "b"}},
                    {"foo", {"foo", "bar", "b", ""}},
                    {"2.1", {"2", "2.1", "1.3", ""}},
                    {"3", {"2", "2.1", "1.3", "3"}}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "ruleset_flavors: "
                 << ::testing::PrintToString(test_case.ruleset_flavors));

    std::vector<Configuration> configs;
    for (const auto& ruleset_flavor : test_case.ruleset_flavors) {
      Configuration config;
      config.general_settings.ruleset_flavor = ruleset_flavor;
      configs.push_back(std::move(config));
    }

    subresource_filter::testing::ScopedSubresourceFilterConfigurator
        scoped_configuration(std::move(configs));
    EXPECT_EQ(test_case.expected_ruleset_flavor_selected,
              GetEnabledConfigurations()
                  ->lexicographically_greatest_ruleset_flavor());
  }
}

TEST(SubresourceFilterFeaturesTest, EnabledConfigurations_FeatureDisabled) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      std::map<std::string, std::string>());

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(config_list->configs_by_decreasing_priority(),
              ::testing::ElementsAre(Configuration()));
  EXPECT_EQ(std::string(),
            config_list->lexicographically_greatest_ruleset_flavor());
}

TEST(SubresourceFilterFeaturesTest,
     EnabledConfigurations_FeatureEnabledWithNoParameters) {
  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      std::map<std::string, std::string>());

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(config_list->configs_by_decreasing_priority(),
              ::testing::ElementsAre(Configuration()));
  EXPECT_EQ(std::string(),
            config_list->lexicographically_greatest_ruleset_flavor());
}

TEST(SubresourceFilterFeaturesTest, PresetForLiveRunOnPhishingSites) {
  ExpectPresetCanBeEnabledByName(
      Configuration::MakePresetForLiveRunOnPhishingSites(),
      kPresetLiveRunOnPhishingSites);
  ExpectPresetIsEquivalentToVariationParams(
      Configuration::MakePresetForLiveRunOnPhishingSites(),
      {{kActivationLevelParameterName, kActivationLevelEnabled},
       {kActivationScopeParameterName, kActivationScopeActivationList},
       {kActivationListsParameterName, kActivationListPhishingInterstitial},
       {kActivationPriorityParameterName, "1000"}});
}

TEST(SubresourceFilterFeaturesTest,
     PresetForPerformanceTestingDryRunOnAllSites) {
  ExpectPresetCanBeEnabledByName(
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      kPresetPerformanceTestingDryRunOnAllSites);
  ExpectPresetIsEquivalentToVariationParams(
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      {{kActivationLevelParameterName, kActivationLevelDryRun},
       {kActivationScopeParameterName, kActivationScopeAllSites},
       {kActivationPriorityParameterName, "500"},
       {kPerformanceMeasurementRateParameterName, "1.0"}});
}

TEST(SubresourceFilterFeaturesTest, ConfigurationPriorities) {
  const std::vector<Configuration> expected_order_by_decreasing_priority = {
      Configuration::MakePresetForLiveRunOnPhishingSites(),
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      Configuration() /* default constructor */
  };

  std::vector<Configuration> shuffled_order = {
      expected_order_by_decreasing_priority[2],
      expected_order_by_decreasing_priority[0],
      expected_order_by_decreasing_priority[1]};
  subresource_filter::testing::ScopedSubresourceFilterConfigurator
      scoped_configuration(std::move(shuffled_order));
  EXPECT_THAT(
      GetEnabledConfigurations()->configs_by_decreasing_priority(),
      ::testing::ElementsAreArray(expected_order_by_decreasing_priority));
}

TEST(SubresourceFilterFeaturesTest, EnableDisableMultiplePresets) {
  const std::string kPhishing(kPresetLiveRunOnPhishingSites);
  const std::string kPerfTest(kPresetPerformanceTestingDryRunOnAllSites);

  // The default config comes from the empty experimental configuration.
  const std::vector<Configuration> kDefaultConfig = {Configuration()};
  const std::vector<Configuration> kPhishingAndDefaultConfigs = {
      Configuration::MakePresetForLiveRunOnPhishingSites(), Configuration()};
  const std::vector<Configuration> kAllConfigs = {
      Configuration::MakePresetForLiveRunOnPhishingSites(),
      Configuration::MakePresetForPerformanceTestingDryRunOnAllSites(),
      Configuration()};

  const struct {
    std::string enable_preset_name_list;
    std::string disable_preset_name_list;
    const std::vector<Configuration> expected_configs;
  } kTestCases[] = {
      {"", "", kDefaultConfig},
      {"garbage1", "garbage2", kDefaultConfig},
      {"", kPhishing + "," + kPerfTest, kDefaultConfig},
      {kPhishing, kPerfTest, kPhishingAndDefaultConfigs},
      {kPhishing + "," + kPerfTest, "garbage", kAllConfigs},
      {kPerfTest + "," + kPhishing, base::ToUpperASCII(kPerfTest),
       kPhishingAndDefaultConfigs},
      {kPerfTest + "," + kPhishing,
       ",,garbage, ," + kPerfTest + "," + kPhishing, kDefaultConfig},
      {base::ToUpperASCII(kPhishing) + "," + base::ToUpperASCII(kPerfTest), "",
       kAllConfigs},
      {",, ," + kPerfTest + ",," + kPhishing, "", kAllConfigs},
      {"garbage,garbage2," + kPerfTest + "," + kPhishing, "", kAllConfigs}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(
        ::testing::Message()
        << "enable_preset_name_list: " << test_case.enable_preset_name_list
        << " disable_preset_name_list: " << test_case.disable_preset_name_list);

    ScopedExperimentalStateToggle scoped_experimental_state(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        {{kEnablePresetsParameterName, test_case.enable_preset_name_list},
         {kDisablePresetsParameterName, test_case.disable_preset_name_list}});

    const auto config_list = GetEnabledConfigurations();
    EXPECT_THAT(config_list->configs_by_decreasing_priority(),
                ::testing::ElementsAreArray(test_case.expected_configs));
    EXPECT_EQ(std::string(),
              config_list->lexicographically_greatest_ruleset_flavor());
  }
}

TEST(SubresourceFilterFeaturesTest,
     EnableMultiplePresetsAndExperimentalConfig) {
  const std::string kPhishing(kPresetLiveRunOnPhishingSites);
  const std::string kPerfTest(kPresetPerformanceTestingDryRunOnAllSites);
  const std::string kTestRulesetFlavor("foobar");

  ScopedExperimentalStateToggle scoped_experimental_state(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      {{kEnablePresetsParameterName, kPhishing + "," + kPerfTest},
       {kActivationLevelParameterName, kActivationLevelDryRun},
       {kActivationScopeParameterName, kActivationScopeActivationList},
       {kActivationListsParameterName, kActivationListSubresourceFilter},
       {kActivationPriorityParameterName, "750"},
       {kRulesetFlavorParameterName, kTestRulesetFlavor}});

  Configuration experimental_config(ActivationLevel::DRYRUN,
                                    ActivationScope::ACTIVATION_LIST,
                                    ActivationList::SUBRESOURCE_FILTER);
  experimental_config.activation_conditions.priority = 750;
  experimental_config.general_settings.ruleset_flavor = kTestRulesetFlavor;

  const auto config_list = GetEnabledConfigurations();
  EXPECT_THAT(
      config_list->configs_by_decreasing_priority(),
      ::testing::ElementsAre(
          Configuration::MakePresetForLiveRunOnPhishingSites(),
          experimental_config,
          Configuration::MakePresetForPerformanceTestingDryRunOnAllSites()));
  EXPECT_EQ(kTestRulesetFlavor,
            config_list->lexicographically_greatest_ruleset_flavor());
}

}  // namespace subresource_filter

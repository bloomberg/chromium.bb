// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/omnibox_field_trial.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxFieldTrialTest : public testing::Test {
 public:
  OmniboxFieldTrialTest() {
    ResetFieldTrialList();
  }

  void ResetFieldTrialList() {
    // Destroy the existing FieldTrialList before creating a new one to avoid
    // a DCHECK.
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("foo")));
    chrome_variations::testing::ClearAllVariationParams();
    OmniboxFieldTrial::ActivateDynamicTrials();
  }

  // Creates and activates a field trial.
  static base::FieldTrial* CreateTestTrial(const std::string& name,
                                           const std::string& group_name) {
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        name, group_name);
    trial->group();
    return trial;
  }

  // EXPECTS that demotions[match_type] exists with value expected_value.
  static void VerifyDemotion(
      const OmniboxFieldTrial::DemotionMultipliers& demotions,
      AutocompleteMatchType::Type match_type,
      float expected_value);

  // EXPECT()s that OmniboxFieldTrial::GetValueForRuleInContext(|rule|,
  // |page_classification|) returns |rule_value|.
  static void ExpectRuleValue(
      const std::string& rule_value,
      const std::string& rule,
      AutocompleteInput::PageClassification page_classification);

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxFieldTrialTest);
};

// static
void OmniboxFieldTrialTest::VerifyDemotion(
    const OmniboxFieldTrial::DemotionMultipliers& demotions,
    AutocompleteMatchType::Type match_type,
    float expected_value) {
  OmniboxFieldTrial::DemotionMultipliers::const_iterator demotion_it =
      demotions.find(match_type);
  ASSERT_TRUE(demotion_it != demotions.end());
  EXPECT_FLOAT_EQ(expected_value, demotion_it->second);
}

// static
void OmniboxFieldTrialTest::ExpectRuleValue(
    const std::string& rule_value,
    const std::string& rule,
    AutocompleteInput::PageClassification page_classification) {
  EXPECT_EQ(rule_value,
            OmniboxFieldTrial::GetValueForRuleInContext(
                rule, page_classification));
}

// Test if GetDisabledProviderTypes() properly parses various field trial
// group names.
TEST_F(OmniboxFieldTrialTest, GetDisabledProviderTypes) {
  EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());

  {
    SCOPED_TRACE("Invalid groups");
    CreateTestTrial("AutocompleteDynamicTrial_0", "DisabledProviders_");
    EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());
    ResetFieldTrialList();
    CreateTestTrial("AutocompleteDynamicTrial_1", "DisabledProviders_XXX");
    EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());
    ResetFieldTrialList();
    CreateTestTrial("AutocompleteDynamicTrial_1", "DisabledProviders_12abc");
    EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());
  }

  {
    SCOPED_TRACE("Valid group name, unsupported trial name.");
    ResetFieldTrialList();
    CreateTestTrial("UnsupportedTrialName", "DisabledProviders_20");
    EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());
  }

  {
    SCOPED_TRACE("Valid field and group name.");
    ResetFieldTrialList();
    CreateTestTrial("AutocompleteDynamicTrial_2", "DisabledProviders_3");
    EXPECT_EQ(3, OmniboxFieldTrial::GetDisabledProviderTypes());
    // Two groups should be OR-ed together.
    CreateTestTrial("AutocompleteDynamicTrial_3", "DisabledProviders_6");
    EXPECT_EQ(7, OmniboxFieldTrial::GetDisabledProviderTypes());
  }
}

// Test if InZeroSuggestFieldTrial() properly parses various field trial
// group names.
TEST_F(OmniboxFieldTrialTest, ZeroSuggestFieldTrial) {
  EXPECT_FALSE(OmniboxFieldTrial::InZeroSuggestFieldTrial());

  {
    SCOPED_TRACE("Valid group name, unsupported trial name.");
    ResetFieldTrialList();
    CreateTestTrial("UnsupportedTrialName", "EnableZeroSuggest");
    EXPECT_FALSE(OmniboxFieldTrial::InZeroSuggestFieldTrial());

    ResetFieldTrialList();
    CreateTestTrial("UnsupportedTrialName", "EnableZeroSuggest_Queries");
    EXPECT_FALSE(OmniboxFieldTrial::InZeroSuggestFieldTrial());

    ResetFieldTrialList();
    CreateTestTrial("UnsupportedTrialName", "EnableZeroSuggest_URLS");
    EXPECT_FALSE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
  }

  {
    SCOPED_TRACE("Valid trial name, unsupported group name.");
    ResetFieldTrialList();
    CreateTestTrial("AutocompleteDynamicTrial_2", "UnrelatedGroup");
    EXPECT_FALSE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
  }

  {
    SCOPED_TRACE("Valid field and group name.");
    ResetFieldTrialList();
    CreateTestTrial("AutocompleteDynamicTrial_2", "EnableZeroSuggest");
    EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());

    ResetFieldTrialList();
    CreateTestTrial("AutocompleteDynamicTrial_2", "EnableZeroSuggest_Queries");
    EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());

    ResetFieldTrialList();
    CreateTestTrial("AutocompleteDynamicTrial_3", "EnableZeroSuggest_URLs");
    EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
  }
}

TEST_F(OmniboxFieldTrialTest, GetDemotionsByTypeWithFallback) {
  {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":1:*"] =
        "1:50,2:0";
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":3:*"] =
        "5:100";
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":*:*"] = "1:25";
    ASSERT_TRUE(chrome_variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
  OmniboxFieldTrial::DemotionMultipliers demotions_by_type;
  OmniboxFieldTrial::GetDemotionsByType(
      AutocompleteInput::NTP, &demotions_by_type);
  ASSERT_EQ(2u, demotions_by_type.size());
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::HISTORY_URL, 0.5);
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::HISTORY_TITLE, 0.0);
  OmniboxFieldTrial::GetDemotionsByType(
      AutocompleteInput::HOME_PAGE, &demotions_by_type);
  ASSERT_EQ(1u, demotions_by_type.size());
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::NAVSUGGEST, 1.0);
  OmniboxFieldTrial::GetDemotionsByType(
      AutocompleteInput::BLANK, &demotions_by_type);
  ASSERT_EQ(1u, demotions_by_type.size());
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::HISTORY_URL, 0.25);
}

TEST_F(OmniboxFieldTrialTest, GetUndemotableTopTypes) {
  {
    std::map<std::string, std::string> params;
    const std::string rule(OmniboxFieldTrial::kUndemotableTopTypeRule);
    params[rule + ":1:*"] = "1,3";
    params[rule + ":3:*"] = "5";
    params[rule + ":*:*"] = "2";
    ASSERT_TRUE(chrome_variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
  OmniboxFieldTrial::UndemotableTopMatchTypes undemotable_types;
  undemotable_types = OmniboxFieldTrial::GetUndemotableTopTypes(
      AutocompleteInput::NTP);
  ASSERT_EQ(2u, undemotable_types.size());
  ASSERT_EQ(1u, undemotable_types.count(AutocompleteMatchType::HISTORY_URL));
  ASSERT_EQ(1u, undemotable_types.count(AutocompleteMatchType::HISTORY_BODY));
  undemotable_types = OmniboxFieldTrial::GetUndemotableTopTypes(
      AutocompleteInput::HOME_PAGE);
  ASSERT_EQ(1u, undemotable_types.size());
  ASSERT_EQ(1u, undemotable_types.count(AutocompleteMatchType::NAVSUGGEST));
  undemotable_types = OmniboxFieldTrial::GetUndemotableTopTypes(
      AutocompleteInput::BLANK);
  ASSERT_EQ(1u, undemotable_types.size());
  ASSERT_EQ(1u, undemotable_types.count(AutocompleteMatchType::HISTORY_TITLE));
}

TEST_F(OmniboxFieldTrialTest, GetValueForRuleInContext) {
  // This test starts with Instant Extended off (the default state), then
  // enables Instant Extended and tests again on the same rules.

  {
    std::map<std::string, std::string> params;
    // Rule 1 has some exact matches and fallbacks at every level.
    params["rule1:1:0"] = "rule1-1-0-value";  // NTP
    params["rule1:3:0"] = "rule1-3-0-value";  // HOME_PAGE
    params["rule1:4:1"] = "rule1-4-1-value";  // OTHER
    params["rule1:4:*"] = "rule1-4-*-value";  // OTHER
    params["rule1:*:1"] = "rule1-*-1-value";  // global
    params["rule1:*:*"] = "rule1-*-*-value";  // global
    // Rule 2 has no exact matches but has fallbacks.
    params["rule2:*:0"] = "rule2-*-0-value";  // global
    params["rule2:1:*"] = "rule2-1-*-value";  // NTP
    params["rule2:*:*"] = "rule2-*-*-value";  // global
    // Rule 3 has only a global fallback.
    params["rule3:*:*"] = "rule3-*-*-value";  // global
    // Rule 4 has an exact match but no fallbacks.
    params["rule4:4:0"] = "rule4-4-0-value";  // OTHER
    // Add a malformed rule to make sure it doesn't screw things up.
    params["unrecognized"] = "unrecognized-value";
    ASSERT_TRUE(chrome_variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }

  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  // Tests with Instant Extended disabled.
  // Tests for rule 1.
  ExpectRuleValue("rule1-1-0-value",
                  "rule1", AutocompleteInput::NTP);  // exact match
  ExpectRuleValue("rule1-1-0-value",
                  "rule1", AutocompleteInput::NTP);  // exact match
  ExpectRuleValue("rule1-*-*-value",
                  "rule1", AutocompleteInput::BLANK);      // fallback to global
  ExpectRuleValue("rule1-3-0-value",
                  "rule1", AutocompleteInput::HOME_PAGE);  // exact match
  ExpectRuleValue("rule1-4-*-value",
                  "rule1", AutocompleteInput::OTHER);      // partial fallback
  ExpectRuleValue("rule1-*-*-value",
                  "rule1",
                  AutocompleteInput::                      // fallback to global
                      SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT);
  // Tests for rule 2.
  ExpectRuleValue("rule2-*-0-value",
                  "rule2", AutocompleteInput::HOME_PAGE);  // partial fallback
  ExpectRuleValue("rule2-*-0-value",
                  "rule2", AutocompleteInput::OTHER);      // partial fallback

  // Tests for rule 3.
  ExpectRuleValue("rule3-*-*-value",
                  "rule3", AutocompleteInput::HOME_PAGE);  // fallback to global
  ExpectRuleValue("rule3-*-*-value",
                  "rule3", AutocompleteInput::OTHER);      // fallback to global

  // Tests for rule 4.
  ExpectRuleValue("",
                  "rule4", AutocompleteInput::BLANK);      // no global fallback
  ExpectRuleValue("",
                  "rule4", AutocompleteInput::HOME_PAGE);  // no global fallback
  ExpectRuleValue("rule4-4-0-value",
                  "rule4", AutocompleteInput::OTHER);      // exact match

  // Tests for rule 5 (a missing rule).
  ExpectRuleValue("",
                  "rule5", AutocompleteInput::OTHER);      // no rule at all

  // Now change the Instant Extended state and run analogous tests.
  // Instant Extended only works on non-mobile platforms.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  chrome::ResetInstantExtendedOptInStateGateForTest();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableInstantExtendedAPI);

  // Tests with Instant Extended enabled.
  // Tests for rule 1.
  ExpectRuleValue("rule1-4-1-value",
                  "rule1", AutocompleteInput::OTHER);      // exact match
  ExpectRuleValue("rule1-*-1-value",
                  "rule1", AutocompleteInput::BLANK);      // partial fallback
  ExpectRuleValue("rule1-*-1-value",
                  "rule1",
                  AutocompleteInput::NTP);        // partial fallback

  // Tests for rule 2.
  ExpectRuleValue("rule2-1-*-value",
                  "rule2",
                  AutocompleteInput::NTP);        // partial fallback
  ExpectRuleValue("rule2-*-*-value",
                  "rule2", AutocompleteInput::OTHER);      // global fallback

  // Tests for rule 3.
  ExpectRuleValue("rule3-*-*-value",
                  "rule3", AutocompleteInput::HOME_PAGE);  // global fallback
  ExpectRuleValue("rule3-*-*-value",
                  "rule3", AutocompleteInput::OTHER);      // global fallback

  // Tests for rule 4.
  ExpectRuleValue("",
                  "rule4", AutocompleteInput::BLANK);      // no global fallback
  ExpectRuleValue("",
                  "rule4", AutocompleteInput::HOME_PAGE);  // no global fallback

  // Tests for rule 5 (a missing rule).
  ExpectRuleValue("",
                  "rule5", AutocompleteInput::OTHER);      // no rule at all
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)
}

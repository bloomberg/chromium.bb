// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "chrome/browser/metrics/variations/variations_registry_syncer_win.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(VariationsRegistrySyncer, BuildGoogleUpdateExperimentLabel) {
  struct {
    const char* active_group_pairs;
    const wchar_t* expected_ids;
  } test_cases[] = {
    // Empty group.
    {"", L""},
    // Group of 1.
    {"FieldTrialA#Default", L"3300200"},
    // Group of 1, doesn't have an associated ID.
    {"FieldTrialA#DoesNotExist", L""},
    // Group of 3.
    {"FieldTrialA#Default#FieldTrialB#Group1#FieldTrialC#Default",
     L"3300200#3300201#3300202"},
    // Group of 3, one doesn't have an associated ID.
    {"FieldTrialA#Default#FieldTrialB#DoesNotExist#FieldTrialC#Default",
     L"3300200#3300202"},
    // Group of 3, all three don't have an associated ID.
    {"FieldTrialX#Default#FieldTrialB#DoesNotExist#FieldTrialC#Default",
     L"3300202"},
  };

  // Register a few VariationIDs.
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE, "FieldTrialA", "Default",
          chrome_variations::TEST_VALUE_A);
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE, "FieldTrialB", "Group1",
          chrome_variations::TEST_VALUE_B);
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE, "FieldTrialC", "Default",
          chrome_variations::TEST_VALUE_C);
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE, "FieldTrialD", "Default",
          chrome_variations::TEST_VALUE_D);  // Not actually used.

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    // Parse the input groups.
    base::FieldTrial::ActiveGroups groups;
    std::vector<std::string> group_data;
    base::SplitString(test_cases[i].active_group_pairs, '#', &group_data);
    ASSERT_EQ(0, group_data.size() % 2);
    for (size_t j = 0; j < group_data.size(); j += 2) {
      base::FieldTrial::ActiveGroup group;
      group.trial_name = group_data[j];
      group.group_name = group_data[j + 1];
      groups.push_back(group);
    }

    // Parse the expected output.
    std::vector<string16> expected_ids_list;
    base::SplitString(test_cases[i].expected_ids, '#', &expected_ids_list);

    string16 experiment_labels_string =
        chrome_variations::VariationsRegistrySyncer::
            BuildGoogleUpdateExperimentLabel(groups);

    // Split the VariationIDs from the labels for verification below.
    std::vector<string16> split_labels;
    std::set<string16> parsed_ids;
    base::SplitString(experiment_labels_string, ';', &split_labels);
    for (std::vector<string16>::const_iterator it = split_labels.begin();
         it != split_labels.end(); ++it) {
      // The ID is precisely between the '=' and '|' characters in each label.
      size_t index_of_equals = it->find('=');
      size_t index_of_pipe = it->find('|');
      ASSERT_NE(string16::npos, index_of_equals);
      ASSERT_NE(string16::npos, index_of_pipe);
      ASSERT_GT(index_of_pipe, index_of_equals);
      parsed_ids.insert(it->substr(index_of_equals + 1,
                                   index_of_pipe - index_of_equals - 1));
    }

    // Verify that the resulting string contains each of the expected labels,
    // and nothing more. Note that the date is stripped out and ignored.
    for (std::vector<string16>::const_iterator it =
             expected_ids_list.begin(); it != expected_ids_list.end(); ++it) {
      std::set<string16>::iterator it2 = parsed_ids.find(*it);
      EXPECT_NE(parsed_ids.end(), it2);
      parsed_ids.erase(it2);
    }
    EXPECT_TRUE(parsed_ids.empty());
  }  // for
}

TEST(VariationsRegistrySyncer, CombineExperimentLabels) {
  struct {
    const wchar_t* variations_labels;
    const wchar_t* other_labels;
    const wchar_t* expected_label;
  } test_cases[] = {
    {L"A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     L"C=D|Tue, 21 Jan 2014 15:30:21 GMT",
     L"C=D|Tue, 21 Jan 2014 15:30:21 GMT;A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {L"A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     L"",
     L"A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {L"",
     L"A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     L"A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {L"A=B|Tue, 21 Jan 2014 15:30:21 GMT;C=D|Tue, 21 Jan 2014 15:30:21 GMT",
     L"P=Q|Tue, 21 Jan 2014 15:30:21 GMT;X=Y|Tue, 21 Jan 2014 15:30:21 GMT",
     L"P=Q|Tue, 21 Jan 2014 15:30:21 GMT;X=Y|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"A=B|Tue, 21 Jan 2014 15:30:21 GMT;C=D|Tue, 21 Jan 2014 15:30:21 GMT"},
    {L"",
     L"",
     L""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    string16 result =
        chrome_variations::VariationsRegistrySyncer::CombineExperimentLabels(
            test_cases[i].variations_labels, test_cases[i].other_labels);
    EXPECT_EQ(test_cases[i].expected_label, result);
  }
}

TEST(VariationsRegistrySyncer, ExtractNonVariationLabels) {
  struct {
    const wchar_t* input_label;
    const wchar_t* expected_output;
  } test_cases[] = {
    // Empty
    {L"", L""},
    // One
    {L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT",
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Three
    {L"CrVar1=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"experiment1=456|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"experiment2=789|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar1=123|Tue, 21 Jan 2014 15:30:21 GMT",
     L"experiment1=456|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"experiment2=789|Tue, 21 Jan 2014 15:30:21 GMT"},
    // One and one Variation
    {L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT",
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // One and one Variation, flipped
    {L"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT",
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Sandwiched
    {L"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar2=3310003|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar3=3310004|Tue, 21 Jan 2014 15:30:21 GMT",
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Only Variations
    {L"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar2=3310003|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar3=3310004|Tue, 21 Jan 2014 15:30:21 GMT",
     L""},
    // Empty values
    {L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT",
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Trailing semicolon
    {L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     L"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;",  // Note the semi here.
     L"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Semis
    {L";;;;", L""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    string16 non_variation_labels =
        chrome_variations::VariationsRegistrySyncer::ExtractNonVariationLabels(
            test_cases[i].input_label);
    EXPECT_EQ(test_cases[i].expected_output, non_variation_labels);
  }
}

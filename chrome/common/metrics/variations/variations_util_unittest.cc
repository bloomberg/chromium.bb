// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Variations Helpers.

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/metrics/metrics_util.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

namespace {

// Convenience helper to retrieve the chrome_variations::VariationID for a
// FieldTrial. Note that this will do the group assignment in |trial| if not
// already done.
VariationID GetIDForTrial(IDCollectionKey key, base::FieldTrial* trial) {
  return GetGoogleVariationID(key, trial->trial_name(), trial->group_name());
}

}  // namespace

class VariationsUtilTest : public ::testing::Test {
 public:
  VariationsUtilTest() : field_trial_list_(NULL) {
    // Since the API can only be called on the UI thread, we have to fake that
    // we're on it.
    ui_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::UI, &message_loop_));

    base::Time now = base::Time::NowFromSystemTime();
    base::TimeDelta one_year = base::TimeDelta::FromDays(365);
    base::Time::Exploded exploded;

    base::Time next_year_time = now + one_year;
    next_year_time.LocalExplode(&exploded);
    next_year_ = exploded.year;

    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
  }

 protected:
  int next_year_;

 private:
  base::FieldTrialList field_trial_list_;
  MessageLoop message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
};

TEST_F(VariationsUtilTest, GetFieldTrialActiveGroups) {
  typedef std::set<ActiveGroupId, ActiveGroupIdCompare> ActiveGroupIdSet;
  std::string trial_one("trial one");
  std::string group_one("group one");
  std::string trial_two("trial two");
  std::string group_two("group two");

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrial::ActiveGroup active_group;
  active_group.trial_name = trial_one;
  active_group.group_name = group_one;
  active_groups.push_back(active_group);

  active_group.trial_name = trial_two;
  active_group.group_name = group_two;
  active_groups.push_back(active_group);

  // Create our expected groups of IDs.
  ActiveGroupIdSet expected_groups;
  ActiveGroupId name_group_id;
  name_group_id.name = metrics::HashName(trial_one);
  name_group_id.group = metrics::HashName(group_one);
  expected_groups.insert(name_group_id);
  name_group_id.name = metrics::HashName(trial_two);
  name_group_id.group = metrics::HashName(group_two);
  expected_groups.insert(name_group_id);

  std::vector<ActiveGroupId> active_group_ids;
  testing::TestGetFieldTrialActiveGroupIds(active_groups, &active_group_ids);
  EXPECT_EQ(2U, active_group_ids.size());
  for (size_t i = 0; i < active_group_ids.size(); ++i) {
    ActiveGroupIdSet::iterator expected_group =
        expected_groups.find(active_group_ids[i]);
    EXPECT_FALSE(expected_group == expected_groups.end());
    expected_groups.erase(expected_group);
  }
  EXPECT_EQ(0U, expected_groups.size());
}

// Test that if the trial is immediately disabled, GetGoogleVariationID just
// returns the empty ID.
TEST_F(VariationsUtilTest, DisableImmediately) {
  int default_group_number = -1;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("trial", 100, "default",
                                                 next_year_, 12, 12,
                                                 &default_group_number));
  trial->Disable();

  ASSERT_EQ(default_group_number, trial->group());
  ASSERT_EQ(EMPTY_ID, GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial.get()));
}

// Test that successfully associating the FieldTrial with some ID, and then
// disabling the FieldTrial actually makes GetGoogleVariationID correctly
// return the empty ID.
TEST_F(VariationsUtilTest, DisableAfterInitialization) {
  const std::string default_name = "default";
  const std::string non_default_name = "non_default";

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("trial", 100, default_name,
                                                 next_year_, 12, 12, NULL));
  trial->AppendGroup(non_default_name, 100);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial->trial_name(),
      default_name, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial->trial_name(),
      non_default_name, TEST_VALUE_B);
  trial->Disable();
  ASSERT_EQ(default_name, trial->group_name());
  ASSERT_EQ(TEST_VALUE_A, GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial.get()));
}

// Test various successful association cases.
TEST_F(VariationsUtilTest, AssociateGoogleVariationID) {
  const std::string default_name1 = "default";
  scoped_refptr<base::FieldTrial> trial_true(
      base::FieldTrialList::FactoryGetFieldTrial("d1", 10, default_name1,
                                                 next_year_, 12, 31, NULL));
  const std::string winner = "TheWinner";
  int winner_group = trial_true->AppendGroup(winner, 10);

  // Set GoogleVariationIDs so we can verify that they were chosen correctly.
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_true->trial_name(),
      default_name1, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_true->trial_name(),
      winner, TEST_VALUE_B);

  EXPECT_EQ(winner_group, trial_true->group());
  EXPECT_EQ(winner, trial_true->group_name());
  EXPECT_EQ(TEST_VALUE_B,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));

  const std::string default_name2 = "default2";
  scoped_refptr<base::FieldTrial> trial_false(
      base::FieldTrialList::FactoryGetFieldTrial("d2", 10, default_name2,
                                                 next_year_, 12, 31, NULL));
  const std::string loser = "ALoser";
  const int loser_group = trial_false->AppendGroup(loser, 0);

  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_false->trial_name(),
      default_name2, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_false->trial_name(),
      loser, TEST_VALUE_B);

  EXPECT_NE(loser_group, trial_false->group());
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_false.get()));
}

// Test that not associating a FieldTrial with any IDs ensure that the empty ID
// will be returned.
TEST_F(VariationsUtilTest, NoAssociation) {
  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> no_id_trial(
      base::FieldTrialList::FactoryGetFieldTrial("d3", 10, default_name,
                                                 next_year_, 12, 31, NULL));
  const std::string winner = "TheWinner";
  const int winner_group = no_id_trial->AppendGroup(winner, 10);

  // Ensure that despite the fact that a normal winner is elected, it does not
  // have a valid VariationID associated with it.
  EXPECT_EQ(winner_group, no_id_trial->group());
  EXPECT_EQ(winner, no_id_trial->group_name());
  EXPECT_EQ(EMPTY_ID, GetIDForTrial(GOOGLE_WEB_PROPERTIES, no_id_trial.get()));
}

// Ensure that the AssociateGoogleVariationIDForce works as expected.
TEST_F(VariationsUtilTest, ForceAssociation) {
  EXPECT_EQ(EMPTY_ID,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group",
                             TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group",
                             TEST_VALUE_B);
  EXPECT_EQ(TEST_VALUE_A,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
  AssociateGoogleVariationIDForce(GOOGLE_WEB_PROPERTIES, "trial", "group",
                                  TEST_VALUE_B);
  EXPECT_EQ(TEST_VALUE_B,
            GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial", "group"));
}

TEST_F(VariationsUtilTest, GenerateExperimentChunks) {
  const char* kExperimentStrings[] = {
      "1d3048f1-9de009d0",
      "cd73da34-cf196cb",
      "6214fa18-9e6dc24d",
      "4dcb0cd6-d31c4ca1",
      "9d5bce6-30d7d8ac",
  };
  const char* kExpectedChunks1[] = {
      "1d3048f1-9de009d0",
  };
  const char* kExpectedChunks2[] = {
      "1d3048f1-9de009d0,cd73da34-cf196cb",
  };
  const char* kExpectedChunks3[] = {
      "1d3048f1-9de009d0,cd73da34-cf196cb,6214fa18-9e6dc24d",
  };
  const char* kExpectedChunks4[] = {
      "1d3048f1-9de009d0,cd73da34-cf196cb,6214fa18-9e6dc24d",
      "4dcb0cd6-d31c4ca1",
  };
  const char* kExpectedChunks5[] = {
      "1d3048f1-9de009d0,cd73da34-cf196cb,6214fa18-9e6dc24d",
      "4dcb0cd6-d31c4ca1,9d5bce6-30d7d8ac",
  };

  struct {
    size_t strings_length;
    size_t expected_chunks_length;
    const char** expected_chunks;
  } cases[] = {
    { 0, 0, NULL },
    { 1, arraysize(kExpectedChunks1), kExpectedChunks1 },
    { 2, arraysize(kExpectedChunks2), kExpectedChunks2 },
    { 3, arraysize(kExpectedChunks3), kExpectedChunks3 },
    { 4, arraysize(kExpectedChunks4), kExpectedChunks4 },
    { 5, arraysize(kExpectedChunks5), kExpectedChunks5 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    ASSERT_LE(cases[i].strings_length, arraysize(kExperimentStrings));

    std::vector<string16> experiments;
    for (size_t j = 0; j < cases[i].strings_length; ++j)
      experiments.push_back(UTF8ToUTF16(kExperimentStrings[j]));

    std::vector<string16> chunks;
    GenerateVariationChunks(experiments, &chunks);
    ASSERT_EQ(cases[i].expected_chunks_length, chunks.size());
    for (size_t j = 0; j < chunks.size(); ++j)
      EXPECT_EQ(UTF8ToUTF16(cases[i].expected_chunks[j]), chunks[j]);
  }
}

// Ensure that two collections can coexist without affecting each other.
TEST_F(VariationsUtilTest, CollectionsCoexist) {
  const std::string default_name = "default";
  int default_group_number = -1;
  scoped_refptr<base::FieldTrial> trial_true(
      base::FieldTrialList::FactoryGetFieldTrial("d1", 10, default_name,
                                                 next_year_, 12, 31,
                                                 &default_group_number));
  ASSERT_EQ(default_group_number, trial_true->group());
  ASSERT_EQ(default_name, trial_true->group_name());

  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));

  AssociateGoogleVariationID(GOOGLE_WEB_PROPERTIES, trial_true->trial_name(),
      default_name, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));
  EXPECT_EQ(EMPTY_ID,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));

  AssociateGoogleVariationID(GOOGLE_UPDATE_SERVICE, trial_true->trial_name(),
      default_name, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_WEB_PROPERTIES, trial_true.get()));
  EXPECT_EQ(TEST_VALUE_A,
            GetIDForTrial(GOOGLE_UPDATE_SERVICE, trial_true.get()));
}

TEST_F(VariationsUtilTest, BuildGoogleUpdateExperimentLabel) {
  struct {
    const char* active_group_pairs;
    const char* expected_ids;
  } test_cases[] = {
    // Empty group.
    {"", ""},
    // Group of 1.
    {"FieldTrialA#Default", "3300200"},
    // Group of 1, doesn't have an associated ID.
    {"FieldTrialA#DoesNotExist", ""},
    // Group of 3.
    {"FieldTrialA#Default#FieldTrialB#Group1#FieldTrialC#Default",
     "3300200#3300201#3300202"},
    // Group of 3, one doesn't have an associated ID.
    {"FieldTrialA#Default#FieldTrialB#DoesNotExist#FieldTrialC#Default",
     "3300200#3300202"},
    // Group of 3, all three don't have an associated ID.
    {"FieldTrialX#Default#FieldTrialB#DoesNotExist#FieldTrialC#Default",
     "3300202"},
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
    ASSERT_EQ(0U, group_data.size() % 2);
    for (size_t j = 0; j < group_data.size(); j += 2) {
      base::FieldTrial::ActiveGroup group;
      group.trial_name = group_data[j];
      group.group_name = group_data[j + 1];
      groups.push_back(group);
    }

    // Parse the expected output.
    std::vector<std::string> expected_ids_list;
    base::SplitString(test_cases[i].expected_ids, '#', &expected_ids_list);

    std::string experiment_labels_string = UTF16ToUTF8(
        BuildGoogleUpdateExperimentLabel(groups));

    // Split the VariationIDs from the labels for verification below.
    std::vector<std::string> split_labels;
    std::set<std::string> parsed_ids;
    base::SplitString(experiment_labels_string, ';', &split_labels);
    for (std::vector<std::string>::const_iterator it = split_labels.begin();
         it != split_labels.end(); ++it) {
      // The ID is precisely between the '=' and '|' characters in each label.
      size_t index_of_equals = it->find('=');
      size_t index_of_pipe = it->find('|');
      ASSERT_NE(std::string::npos, index_of_equals);
      ASSERT_NE(std::string::npos, index_of_pipe);
      ASSERT_GT(index_of_pipe, index_of_equals);
      parsed_ids.insert(it->substr(index_of_equals + 1,
                                   index_of_pipe - index_of_equals - 1));
    }

    // Verify that the resulting string contains each of the expected labels,
    // and nothing more. Note that the date is stripped out and ignored.
    for (std::vector<std::string>::const_iterator it =
             expected_ids_list.begin(); it != expected_ids_list.end(); ++it) {
      std::set<std::string>::iterator it2 = parsed_ids.find(*it);
      EXPECT_TRUE(parsed_ids.end() != it2);
      parsed_ids.erase(it2);
    }
    EXPECT_TRUE(parsed_ids.empty());
  }  // for
}

TEST_F(VariationsUtilTest, CombineExperimentLabels) {
  struct {
    const char* variations_labels;
    const char* other_labels;
    const char* expected_label;
  } test_cases[] = {
    {"A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     "C=D|Tue, 21 Jan 2014 15:30:21 GMT",
     "C=D|Tue, 21 Jan 2014 15:30:21 GMT;A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     "",
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"",
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT",
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"A=B|Tue, 21 Jan 2014 15:30:21 GMT;C=D|Tue, 21 Jan 2014 15:30:21 GMT",
     "P=Q|Tue, 21 Jan 2014 15:30:21 GMT;X=Y|Tue, 21 Jan 2014 15:30:21 GMT",
     "P=Q|Tue, 21 Jan 2014 15:30:21 GMT;X=Y|Tue, 21 Jan 2014 15:30:21 GMT;"
     "A=B|Tue, 21 Jan 2014 15:30:21 GMT;C=D|Tue, 21 Jan 2014 15:30:21 GMT"},
    {"",
     "",
     ""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::string result = UTF16ToUTF8(CombineExperimentLabels(
        ASCIIToUTF16(test_cases[i].variations_labels),
        ASCIIToUTF16(test_cases[i].other_labels)));
    EXPECT_EQ(test_cases[i].expected_label, result);
  }
}

TEST_F(VariationsUtilTest, ExtractNonVariationLabels) {
  struct {
    const char* input_label;
    const char* expected_output;
  } test_cases[] = {
    // Empty
    {"", ""},
    // One
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Three
    {"CrVar1=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "experiment1=456|Tue, 21 Jan 2014 15:30:21 GMT;"
     "experiment2=789|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=123|Tue, 21 Jan 2014 15:30:21 GMT",
     "experiment1=456|Tue, 21 Jan 2014 15:30:21 GMT;"
     "experiment2=789|Tue, 21 Jan 2014 15:30:21 GMT"},
    // One and one Variation
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // One and one Variation, flipped
    {"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Sandwiched
    {"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar2=3310003|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar3=3310004|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Only Variations
    {"CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar2=3310003|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar3=3310004|Tue, 21 Jan 2014 15:30:21 GMT",
     ""},
    // Empty values
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT",
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Trailing semicolon
    {"gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT;"
     "CrVar1=3310002|Tue, 21 Jan 2014 15:30:21 GMT;",  // Note the semi here.
     "gcapi_brand=123|Tue, 21 Jan 2014 15:30:21 GMT"},
    // Semis
    {";;;;", ""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::string non_variation_labels = UTF16ToUTF8(
        ExtractNonVariationLabels(ASCIIToUTF16(test_cases[i].input_label)));
    EXPECT_EQ(test_cases[i].expected_output, non_variation_labels);
  }
}

}  // namespace chrome_variations

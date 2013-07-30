// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Variations Helpers.

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
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

// Tests whether a field trial is active (i.e. group() has been called on it).
bool IsFieldTrialActive(const std::string& trial_name) {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  for (size_t i = 0; i < active_groups.size(); ++i) {
    if (active_groups[i].trial_name == trial_name)
      return true;
  }
  return false;
}

// Call FieldTrialList::FactoryGetFieldTrial() with a future expiry date.
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const std::string& trial_name,
    int total_probability,
    const std::string& default_group_name,
    int* default_group_number) {
  return base::FieldTrialList::FactoryGetFieldTrial(
      trial_name, total_probability, default_group_name,
      base::FieldTrialList::kNoExpirationYear, 1, 1,
      base::FieldTrial::SESSION_RANDOMIZED, default_group_number);
}

}  // namespace

class VariationsUtilTest : public ::testing::Test {
 public:
  VariationsUtilTest() : field_trial_list_(NULL) {
    // Since the API can only be called on the UI thread, we have to fake that
    // we're on it.
    ui_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::UI, &message_loop_));

    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
  }

 private:
  base::FieldTrialList field_trial_list_;
  base::MessageLoop message_loop_;
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
      CreateFieldTrial("trial", 100, "default", &default_group_number));

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
      CreateFieldTrial("trial", 100, default_name, NULL));

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
      CreateFieldTrial("d1", 10, default_name1, NULL));
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
      CreateFieldTrial("d2", 10, default_name2, NULL));
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
      CreateFieldTrial("d3", 10, default_name, NULL));

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

TEST_F(VariationsUtilTest, AssociateVariationParams) {
  const std::string kTrialName = "AssociateVariationParams";

  {
    std::map<std::string, std::string> params;
    params["a"] = "10";
    params["b"] = "test";
    ASSERT_TRUE(AssociateVariationParams(kTrialName, "A", params));
  }
  {
    std::map<std::string, std::string> params;
    params["a"] = "5";
    ASSERT_TRUE(AssociateVariationParams(kTrialName, "B", params));
  }

  base::FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("5", GetVariationParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetVariationParams(kTrialName, &params));
  EXPECT_EQ(1U, params.size());
  EXPECT_EQ("5", params["a"]);
}

TEST_F(VariationsUtilTest, AssociateVariationParams_Fail) {
  const std::string kTrialName = "AssociateVariationParams_Fail";
  const std::string kGroupName = "A";

  std::map<std::string, std::string> params;
  params["a"] = "10";
  ASSERT_TRUE(AssociateVariationParams(kTrialName, kGroupName, params));
  params["a"] = "1";
  params["b"] = "2";
  ASSERT_FALSE(AssociateVariationParams(kTrialName, kGroupName, params));

  base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  EXPECT_EQ("10", GetVariationParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "b"));
}

TEST_F(VariationsUtilTest, AssociateVariationParams_TrialActiveFail) {
  const std::string kTrialName = "AssociateVariationParams_TrialActiveFail";
  base::FieldTrialList::CreateFieldTrial(kTrialName, "A");
  ASSERT_EQ("A", base::FieldTrialList::FindFullName(kTrialName));

  std::map<std::string, std::string> params;
  params["a"] = "10";
  EXPECT_FALSE(AssociateVariationParams(kTrialName, "B", params));
  EXPECT_FALSE(AssociateVariationParams(kTrialName, "A", params));
}

TEST_F(VariationsUtilTest, AssociateVariationParams_DoesntActivateTrial) {
  const std::string kTrialName = "AssociateVariationParams_DoesntActivateTrial";

  ASSERT_FALSE(IsFieldTrialActive(kTrialName));
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));
  ASSERT_FALSE(IsFieldTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  params["a"] = "10";
  EXPECT_TRUE(AssociateVariationParams(kTrialName, "A", params));
  ASSERT_FALSE(IsFieldTrialActive(kTrialName));
}

TEST_F(VariationsUtilTest, GetVariationParams_NoTrial) {
  const std::string kTrialName = "GetVariationParams_NoParams";

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams(kTrialName, &params));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "y"));
}

TEST_F(VariationsUtilTest, GetVariationParams_NoParams) {
  const std::string kTrialName = "GetVariationParams_NoParams";

  base::FieldTrialList::CreateFieldTrial(kTrialName, "A");

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams(kTrialName, &params));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "y"));
}

TEST_F(VariationsUtilTest, GetVariationParams_ActivatesTrial) {
  const std::string kTrialName = "GetVariationParams_ActivatesTrial";

  ASSERT_FALSE(IsFieldTrialActive(kTrialName));
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));
  ASSERT_FALSE(IsFieldTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetVariationParams(kTrialName, &params));
  ASSERT_TRUE(IsFieldTrialActive(kTrialName));
}

TEST_F(VariationsUtilTest, GetVariationParamValue_ActivatesTrial) {
  const std::string kTrialName = "GetVariationParamValue_ActivatesTrial";

  ASSERT_FALSE(IsFieldTrialActive(kTrialName));
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", NULL));
  ASSERT_FALSE(IsFieldTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  EXPECT_EQ(std::string(), GetVariationParamValue(kTrialName, "x"));
  ASSERT_TRUE(IsFieldTrialActive(kTrialName));
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
      CreateFieldTrial("d1", 10, default_name, &default_group_number));
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

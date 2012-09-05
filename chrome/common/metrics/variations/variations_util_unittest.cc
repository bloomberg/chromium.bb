// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Variations Helpers.

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
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
VariationID GetIDForTrial(base::FieldTrial* trial) {
  return GetGoogleVariationID(trial->name(), trial->group_name());
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
  }

 protected:
  int next_year_;

 private:
  base::FieldTrialList field_trial_list_;
  MessageLoop message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
};

TEST_F(VariationsUtilTest, GetFieldTrialSelectedGroups) {
  typedef std::set<SelectedGroupId, SelectedGroupIdCompare> SelectedGroupIdSet;
  std::string trial_one("trial one");
  std::string group_one("group one");
  std::string trial_two("trial two");
  std::string group_two("group two");

  base::FieldTrial::SelectedGroups selected_groups;
  base::FieldTrial::SelectedGroup selected_group;
  selected_group.trial = trial_one;
  selected_group.group = group_one;
  selected_groups.push_back(selected_group);

  selected_group.trial = trial_two;
  selected_group.group = group_two;
  selected_groups.push_back(selected_group);

  // Create our expected groups of IDs.
  SelectedGroupIdSet expected_groups;
  SelectedGroupId name_group_id;
  name_group_id.name = metrics::HashName(trial_one);
  name_group_id.group = metrics::HashName(group_one);
  expected_groups.insert(name_group_id);
  name_group_id.name = metrics::HashName(trial_two);
  name_group_id.group = metrics::HashName(group_two);
  expected_groups.insert(name_group_id);

  std::vector<SelectedGroupId> selected_group_ids;
  testing::TestGetFieldTrialSelectedGroupIdsForSelectedGroups(
      selected_groups, &selected_group_ids);
  EXPECT_EQ(2U, selected_group_ids.size());
  for (size_t i = 0; i < selected_group_ids.size(); ++i) {
    SelectedGroupIdSet::iterator expected_group =
        expected_groups.find(selected_group_ids[i]);
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
  ASSERT_EQ(kEmptyID, GetIDForTrial(trial.get()));
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
  AssociateGoogleVariationID(trial->name(), default_name, kTestValueA);
  AssociateGoogleVariationID(trial->name(), non_default_name, kTestValueB);
  ASSERT_EQ(non_default_name, trial->group_name());
  ASSERT_EQ(kTestValueB, GetIDForTrial(trial.get()));
  trial->Disable();
  ASSERT_EQ(default_name, trial->group_name());
  ASSERT_EQ(kTestValueA, GetIDForTrial(trial.get()));
}

// Test various successful association cases.
TEST_F(VariationsUtilTest, AssociateGoogleVariationID) {
  const std::string default_name1 = "default1";
  scoped_refptr<base::FieldTrial> trial_true(
      base::FieldTrialList::FactoryGetFieldTrial("d1", 10, default_name1,
                                                 next_year_, 12, 31, NULL));
  const std::string winner = "TheWinner";
  int winner_group = trial_true->AppendGroup(winner, 10);

  // Set GoogleVariationIDs so we can verify that they were chosen correctly.
  AssociateGoogleVariationID(trial_true->name(), default_name1, kTestValueA);
  AssociateGoogleVariationID(trial_true->name(), winner, kTestValueB);

  EXPECT_EQ(winner_group, trial_true->group());
  EXPECT_EQ(winner, trial_true->group_name());
  EXPECT_EQ(kTestValueB, GetIDForTrial(trial_true.get()));

  const std::string default_name2 = "default2";
  scoped_refptr<base::FieldTrial> trial_false(
      base::FieldTrialList::FactoryGetFieldTrial("d2", 10, default_name2,
                                                 next_year_, 12, 31, NULL));
  const std::string loser = "ALoser";
  const int loser_group = trial_false->AppendGroup(loser, 0);

  AssociateGoogleVariationID(trial_false->name(), default_name2, kTestValueA);
  AssociateGoogleVariationID(trial_false->name(), loser, kTestValueB);

  EXPECT_NE(loser_group, trial_false->group());
  EXPECT_EQ(kTestValueA, GetIDForTrial(trial_false.get()));
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
  EXPECT_EQ(kEmptyID, GetIDForTrial(no_id_trial.get()));
}

// Ensure that the AssociateGoogleVariationIDForce works as expected.
TEST_F(VariationsUtilTest, ForceAssociation) {
  EXPECT_EQ(kEmptyID, GetGoogleVariationID("trial", "group"));
  AssociateGoogleVariationID("trial", "group", kTestValueA);
  EXPECT_EQ(kTestValueA, GetGoogleVariationID("trial", "group"));
  AssociateGoogleVariationID("trial", "group", kTestValueB);
  EXPECT_EQ(kTestValueA, GetGoogleVariationID("trial", "group"));
  AssociateGoogleVariationIDForce("trial", "group", kTestValueB);
  EXPECT_EQ(kTestValueB, GetGoogleVariationID("trial", "group"));
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

}  // namespace chrome_variations

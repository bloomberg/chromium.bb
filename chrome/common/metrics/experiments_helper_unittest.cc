// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Experiment Helpers.

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/time.h"
#include "chrome/common/metrics/experiments_helper.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Convenience helper to retrieve the chrome_variations::ID for a FieldTrial.
// Note that this will do the group assignment in |trial| if not already done.
chrome_variations::ID GetIDForTrial(base::FieldTrial* trial) {
  return experiments_helper::GetGoogleVariationID(trial->name(),
                                                  trial->group_name());
}

}  // namespace

class ExperimentsHelperTest : public ::testing::Test {
 public:
  ExperimentsHelperTest() {
    // Since the API can only be called on the UI thread, we have to fake that
    // we're on it.
    ui_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::UI, &message_loop_));

    base::Time now = base::Time::NowFromSystemTime();
    base::TimeDelta oneYear = base::TimeDelta::FromDays(365);
    base::Time::Exploded exploded;

    base::Time next_year_time = now + oneYear;
    next_year_time.LocalExplode(&exploded);
    next_year_ = exploded.year;
  }

 protected:
  int next_year_;

 private:
  MessageLoop message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
};

TEST_F(ExperimentsHelperTest, HashName) {
  // Make sure hashing is stable on all platforms.
  struct {
    const char* name;
    uint32 hash_value;
  } known_hashes[] = {
    {"a", 937752454u},
    {"1", 723085877u},
    {"Trial Name", 2713117220u},
    {"Group Name", 3201815843u},
    {"My Favorite Experiment", 3722155194u},
    {"My Awesome Group Name", 4109503236u},
    {"abcdefghijklmonpqrstuvwxyz", 787728696u},
    {"0123456789ABCDEF", 348858318U}
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(known_hashes); ++i) {
    EXPECT_EQ(known_hashes[i].hash_value,
              testing::TestHashName(known_hashes[i].name));
  }
}

TEST_F(ExperimentsHelperTest, GetFieldTrialSelectedGroups) {
  typedef std::set<experiments_helper::SelectedGroupId,
      experiments_helper::SelectedGroupIdCompare> SelectedGroupIdSet;
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
  experiments_helper::SelectedGroupId name_group_id;
  name_group_id.name = testing::TestHashName(trial_one);
  name_group_id.group = testing::TestHashName(group_one);
  expected_groups.insert(name_group_id);
  name_group_id.name = testing::TestHashName(trial_two);
  name_group_id.group = testing::TestHashName(group_two);
  expected_groups.insert(name_group_id);

  std::vector<experiments_helper::SelectedGroupId> selected_group_ids;
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
TEST_F(ExperimentsHelperTest, DisableImmediately) {
  int default_group_number = -1;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("trial", 100, "default",
                                                 next_year_, 12, 12,
                                                 &default_group_number));
  trial->Disable();

  ASSERT_EQ(default_group_number, trial->group());
  ASSERT_EQ(chrome_variations::kEmptyID, GetIDForTrial(trial.get()));
}

// Test that successfully associating the FieldTrial with some ID, and then
// disabling the FieldTrial actually makes GetGoogleVariationID correctly
// return the empty ID.
TEST_F(ExperimentsHelperTest, DisableAfterInitialization) {
  const std::string default_name = "default";
  const std::string non_default_name = "non_default";

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("trial", 100, default_name,
                                                 next_year_, 12, 12, NULL));
  trial->AppendGroup(non_default_name, 100);
  experiments_helper::AssociateGoogleVariationID(
      trial->name(), default_name, chrome_variations::kTestValueA);
  experiments_helper::AssociateGoogleVariationID(
      trial->name(), non_default_name, chrome_variations::kTestValueB);
  ASSERT_EQ(non_default_name, trial->group_name());
  ASSERT_EQ(chrome_variations::kTestValueB, GetIDForTrial(trial.get()));
  trial->Disable();
  ASSERT_EQ(default_name, trial->group_name());
  ASSERT_EQ(chrome_variations::kTestValueA, GetIDForTrial(trial.get()));
}

// Test various successful association cases.
TEST_F(ExperimentsHelperTest, AssociateGoogleVariationID) {
  const std::string default_name1 = "default1";
  scoped_refptr<base::FieldTrial> trial_true(
      base::FieldTrialList::FactoryGetFieldTrial("d1", 10, default_name1,
                                                 next_year_, 12, 31, NULL));
  const std::string winner = "TheWinner";
  int winner_group = trial_true->AppendGroup(winner, 10);

  // Set GoogleVariationIDs so we can verify that they were chosen correctly.
  experiments_helper::AssociateGoogleVariationID(
      trial_true->name(), default_name1, chrome_variations::kTestValueA);
  experiments_helper::AssociateGoogleVariationID(
      trial_true->name(), winner, chrome_variations::kTestValueB);

  EXPECT_EQ(winner_group, trial_true->group());
  EXPECT_EQ(winner, trial_true->group_name());
  EXPECT_EQ(chrome_variations::kTestValueB, GetIDForTrial(trial_true.get()));

  const std::string default_name2 = "default2";
  scoped_refptr<base::FieldTrial> trial_false(
      base::FieldTrialList::FactoryGetFieldTrial("d2", 10, default_name2,
                                                 next_year_, 12, 31, NULL));
  const std::string loser = "ALoser";
  int loser_group = trial_false->AppendGroup(loser, 0);

  experiments_helper::AssociateGoogleVariationID(
      trial_false->name(), default_name2, chrome_variations::kTestValueA);
  experiments_helper::AssociateGoogleVariationID(
      trial_false->name(), loser, chrome_variations::kTestValueB);

  EXPECT_NE(loser_group, trial_false->group());
  EXPECT_EQ(chrome_variations::kTestValueA, GetIDForTrial(trial_false.get()));
}

// Test that not associating a FieldTrial with any IDs ensure that the empty ID
// will be returned.
TEST_F(ExperimentsHelperTest, NoAssociation) {
  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> no_id_trial(
      base::FieldTrialList::FactoryGetFieldTrial("d3", 10, default_name,
                                                 next_year_, 12, 31, NULL));
  const std::string winner = "TheWinner";
  int winner_group = no_id_trial->AppendGroup(winner, 10);

  // Ensure that despite the fact that a normal winner is elected, it does not
  // have a valid chrome_variations::ID associated with it.
  EXPECT_EQ(winner_group, no_id_trial->group());
  EXPECT_EQ(winner, no_id_trial->group_name());
  EXPECT_EQ(chrome_variations::kEmptyID, GetIDForTrial(no_id_trial.get()));
}

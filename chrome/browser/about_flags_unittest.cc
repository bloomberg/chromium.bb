// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// These two should be two arbitrary, but valid names from about_flags.cc.
const char kFlags1[] = "remoting";
const char kFlags2[] = "print-preview";

// The command line flag corresponding to |kFlags1|.
const char* const kFlag1CommandLine = switches::kEnableRemoting;

namespace about_flags {

class AboutFlagsTest : public ::testing::Test {
 protected:
  AboutFlagsTest() {
    prefs_.RegisterListPref(prefs::kEnabledLabsExperiments);
    testing::ClearState();
  }

  TestingPrefService prefs_;
};

TEST_F(AboutFlagsTest, ChangeNeedsRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetExperimentEnabled(&prefs_, kFlags1, true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveOne) {
  // Add two experiments, check they're there.
  SetExperimentEnabled(&prefs_, kFlags1, true);
  SetExperimentEnabled(&prefs_, kFlags2, true);

  ListValue* experiments_list = prefs_.GetMutableList(
      prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list != NULL);

  ASSERT_EQ(2u, experiments_list->GetSize());

  std::string s0;
  ASSERT_TRUE(experiments_list->GetString(0, &s0));
  std::string s1;
  ASSERT_TRUE(experiments_list->GetString(1, &s1));

  EXPECT_TRUE(s0 == kFlags1 || s1 == kFlags1);
  EXPECT_TRUE(s0 == kFlags2 || s1 == kFlags2);

  // Remove one experiment, check the other's still around.
  SetExperimentEnabled(&prefs_, kFlags2, false);

  experiments_list = prefs_.GetMutableList(prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list != NULL);
  ASSERT_EQ(1u, experiments_list->GetSize());
  ASSERT_TRUE(experiments_list->GetString(0, &s0));
  EXPECT_TRUE(s0 == kFlags1);
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveBoth) {
  // Add two experiments, check the pref exists.
  SetExperimentEnabled(&prefs_, kFlags1, true);
  SetExperimentEnabled(&prefs_, kFlags2, true);
  ListValue* experiments_list = prefs_.GetMutableList(
      prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list != NULL);

  // Remove both, the pref should have been removed completely.
  SetExperimentEnabled(&prefs_, kFlags1, false);
  SetExperimentEnabled(&prefs_, kFlags2, false);
  experiments_list = prefs_.GetMutableList(prefs::kEnabledLabsExperiments);
  EXPECT_TRUE(experiments_list == NULL || experiments_list->GetSize() == 0);
}

TEST_F(AboutFlagsTest, ConvertFlagsToSwitches) {
  SetExperimentEnabled(&prefs_, kFlags1, true);

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("foo");

  EXPECT_TRUE(command_line.HasSwitch("foo"));
  EXPECT_FALSE(command_line.HasSwitch(kFlag1CommandLine));

  ConvertFlagsToSwitches(&prefs_, &command_line);

  EXPECT_TRUE(command_line.HasSwitch("foo"));
  EXPECT_TRUE(command_line.HasSwitch(kFlag1CommandLine));
}

TEST_F(AboutFlagsTest, RemoveFlagSwitches) {
  std::map<std::string, CommandLine::StringType> switch_list;
  switch_list[kFlag1CommandLine] = CommandLine::StringType();
  switch_list[switches::kFlagSwitchesBegin] = CommandLine::StringType();
  switch_list[switches::kFlagSwitchesEnd] = CommandLine::StringType();
  switch_list["foo"] = CommandLine::StringType();

  SetExperimentEnabled(&prefs_, kFlags1, true);

  // This shouldn't do anything before ConvertFlagsToSwitches() wasn't called.
  RemoveFlagsSwitches(&switch_list);
  ASSERT_EQ(4u, switch_list.size());
  EXPECT_TRUE(switch_list.find(kFlag1CommandLine) != switch_list.end());
  EXPECT_TRUE(switch_list.find(switches::kFlagSwitchesBegin) !=
              switch_list.end());
  EXPECT_TRUE(switch_list.find(switches::kFlagSwitchesEnd) !=
              switch_list.end());
  EXPECT_TRUE(switch_list.find("foo") != switch_list.end());

  // Call ConvertFlagsToSwitches(), then RemoveFlagsSwitches() again.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("foo");
  ConvertFlagsToSwitches(&prefs_, &command_line);
  RemoveFlagsSwitches(&switch_list);

  // Now the about:flags-related switch should have been removed.
  ASSERT_EQ(1u, switch_list.size());
  EXPECT_TRUE(switch_list.find("foo") != switch_list.end());
}

}  // namespace about_flags

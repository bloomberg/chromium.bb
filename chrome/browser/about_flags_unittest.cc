// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "grit/chromium_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kFlags1[] = "flag1";
const char kFlags2[] = "flag2";
const char kFlags3[] = "flag3";

const char kSwitch1[] = "switch";
const char kSwitch2[] = "switch2";
const char kSwitch3[] = "switch3";

namespace about_flags {

// The experiments that are set for these tests. The first two experiments are
// supported on the current platform, but the last is only supported on a
// platform other than the current.
static Experiment kExperiments[] = {
  {
    kFlags1,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // Ends up being mapped to the current platform.
    kSwitch1
  },
  {
    kFlags2,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // Ends up being mapped to the current platform.
    kSwitch2
  },
  {
    kFlags3,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // This ends up enabling for an OS other than the current.
    kSwitch3
  },
};

class AboutFlagsTest : public ::testing::Test {
 protected:
  AboutFlagsTest() {
    prefs_.RegisterListPref(prefs::kEnabledLabsExperiments);
#if defined(OS_CHROMEOS)
    prefs_.RegisterBooleanPref(prefs::kLabsMediaplayerEnabled, false);
    prefs_.RegisterBooleanPref(prefs::kLabsAdvancedFilesystemEnabled, false);
    prefs_.RegisterBooleanPref(prefs::kUseVerticalTabs, false);
#endif
    testing::ClearState();
  }

  virtual void SetUp() {
    for (size_t i = 0; i < arraysize(kExperiments); ++i)
      kExperiments[i].supported_platforms = GetCurrentPlatform();

    int os_other_than_current = 1;
    while (os_other_than_current == GetCurrentPlatform())
      os_other_than_current <<= 1;
    kExperiments[2].supported_platforms = os_other_than_current;

    testing::SetExperiments(kExperiments, arraysize(kExperiments));
  }

  virtual void TearDown() {
    testing::SetExperiments(NULL, 0);
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
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));

  ConvertFlagsToSwitches(&prefs_, &command_line);

  EXPECT_TRUE(command_line.HasSwitch("foo"));
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
}

TEST_F(AboutFlagsTest, RemoveFlagSwitches) {
  std::map<std::string, CommandLine::StringType> switch_list;
  switch_list[kSwitch1] = CommandLine::StringType();
  switch_list[switches::kFlagSwitchesBegin] = CommandLine::StringType();
  switch_list[switches::kFlagSwitchesEnd] = CommandLine::StringType();
  switch_list["foo"] = CommandLine::StringType();

  SetExperimentEnabled(&prefs_, kFlags1, true);

  // This shouldn't do anything before ConvertFlagsToSwitches() wasn't called.
  RemoveFlagsSwitches(&switch_list);
  ASSERT_EQ(4u, switch_list.size());
  EXPECT_TRUE(switch_list.find(kSwitch1) != switch_list.end());
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

// Tests enabling experiments that aren't supported on the current platform.
TEST_F(AboutFlagsTest, PersistAndPrune) {
  // Enable exerpiement 1 and experiment 3.
  SetExperimentEnabled(&prefs_, kFlags1, true);
  SetExperimentEnabled(&prefs_, kFlags3, true);
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch3));

  // Convert the flags to switches. Experiment 3 shouldn't be among the switches
  // as it is not applicable to the current platform.
  ConvertFlagsToSwitches(&prefs_, &command_line);
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch3));

  // Experiment 3 should show still be persisted in preferences though.
  scoped_ptr<ListValue> switch_prefs(GetFlagsExperimentsData(&prefs_));
  ASSERT_TRUE(switch_prefs.get());
  EXPECT_EQ(2u, switch_prefs->GetSize());
}

}  // namespace about_flags

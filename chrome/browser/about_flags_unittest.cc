// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kFlags1[] = "flag1";
const char kFlags2[] = "flag2";
const char kFlags3[] = "flag3";
const char kFlags4[] = "flag4";
const char kFlags5[] = "flag5";

const char kSwitch1[] = "switch";
const char kSwitch2[] = "switch2";
const char kSwitch3[] = "switch3";
const char kValueForSwitch2[] = "value_for_switch2";

const char kMultiSwitch1[] = "multi_switch1";
const char kMultiSwitch2[] = "multi_switch2";
const char kValueForMultiSwitch2[] = "value_for_multi_switch2";

const char kEnableDisableValue1[] = "value1";
const char kEnableDisableValue2[] = "value2";

namespace about_flags {

const Experiment::Choice kMultiChoices[] = {
  { IDS_PRODUCT_NAME, "", "" },
  { IDS_PRODUCT_NAME, kMultiSwitch1, "" },
  { IDS_PRODUCT_NAME, kMultiSwitch2, kValueForMultiSwitch2 },
};

// The experiments that are set for these tests. The 3rd experiment is not
// supported on the current platform, all others are.
static Experiment kExperiments[] = {
  {
    kFlags1,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // Ends up being mapped to the current platform.
    Experiment::SINGLE_VALUE,
    kSwitch1,
    "",
    NULL,
    NULL,
    NULL,
    0
  },
  {
    kFlags2,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // Ends up being mapped to the current platform.
    Experiment::SINGLE_VALUE,
    kSwitch2,
    kValueForSwitch2,
    NULL,
    NULL,
    NULL,
    0
  },
  {
    kFlags3,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // This ends up enabling for an OS other than the current.
    Experiment::SINGLE_VALUE,
    kSwitch3,
    "",
    NULL,
    NULL,
    NULL,
    0
  },
  {
    kFlags4,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // Ends up being mapped to the current platform.
    Experiment::MULTI_VALUE,
    "",
    "",
    "",
    "",
    kMultiChoices,
    arraysize(kMultiChoices)
  },
  {
    kFlags5,
    IDS_PRODUCT_NAME,
    IDS_PRODUCT_NAME,
    0,  // Ends up being mapped to the current platform.
    Experiment::ENABLE_DISABLE_VALUE,
    kSwitch1,
    kEnableDisableValue1,
    kSwitch2,
    kEnableDisableValue2,
    NULL,
    3
  },
};

class AboutFlagsTest : public ::testing::Test {
 protected:
  AboutFlagsTest() {
    prefs_.registry()->RegisterListPref(prefs::kEnabledLabsExperiments);
    testing::ClearState();
  }

  virtual void SetUp() OVERRIDE {
    for (size_t i = 0; i < arraysize(kExperiments); ++i)
      kExperiments[i].supported_platforms = GetCurrentPlatform();

    int os_other_than_current = 1;
    while (os_other_than_current == GetCurrentPlatform())
      os_other_than_current <<= 1;
    kExperiments[2].supported_platforms = os_other_than_current;

    testing::SetExperiments(kExperiments, arraysize(kExperiments));
  }

  virtual void TearDown() OVERRIDE {
    testing::SetExperiments(NULL, 0);
  }

  TestingPrefServiceSimple prefs_;
};


TEST_F(AboutFlagsTest, NoChangeNoRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetExperimentEnabled(&prefs_, kFlags1, false);
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, ChangeNeedsRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetExperimentEnabled(&prefs_, kFlags1, true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, MultiFlagChangeNeedsRestart) {
  const Experiment& experiment = kExperiments[3];
  ASSERT_EQ(kFlags4, experiment.internal_name);
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  // Enable the 2nd choice of the multi-value.
  SetExperimentEnabled(&prefs_, experiment.NameForChoice(2), true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
  testing::ClearState();
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  // Enable the default choice now.
  SetExperimentEnabled(&prefs_, experiment.NameForChoice(0), true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveOne) {
  // Add two experiments, check they're there.
  SetExperimentEnabled(&prefs_, kFlags1, true);
  SetExperimentEnabled(&prefs_, kFlags2, true);

  const ListValue* experiments_list = prefs_.GetList(
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

  experiments_list = prefs_.GetList(prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list != NULL);
  ASSERT_EQ(1u, experiments_list->GetSize());
  ASSERT_TRUE(experiments_list->GetString(0, &s0));
  EXPECT_TRUE(s0 == kFlags1);
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveBoth) {
  // Add two experiments, check the pref exists.
  SetExperimentEnabled(&prefs_, kFlags1, true);
  SetExperimentEnabled(&prefs_, kFlags2, true);
  const ListValue* experiments_list = prefs_.GetList(
      prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list != NULL);

  // Remove both, the pref should have been removed completely.
  SetExperimentEnabled(&prefs_, kFlags1, false);
  SetExperimentEnabled(&prefs_, kFlags2, false);
  experiments_list = prefs_.GetList(prefs::kEnabledLabsExperiments);
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
  // Enable experiments 1 and 3.
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
  EXPECT_EQ(arraysize(kExperiments), switch_prefs->GetSize());
}

// Tests that switches which should have values get them in the command
// line.
TEST_F(AboutFlagsTest, CheckValues) {
  // Enable experiments 1 and 2.
  SetExperimentEnabled(&prefs_, kFlags1, true);
  SetExperimentEnabled(&prefs_, kFlags2, true);
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch2));

  // Convert the flags to switches.
  ConvertFlagsToSwitches(&prefs_, &command_line);
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
  EXPECT_EQ(std::string(""), command_line.GetSwitchValueASCII(kSwitch1));
  EXPECT_TRUE(command_line.HasSwitch(kSwitch2));
  EXPECT_EQ(std::string(kValueForSwitch2),
            command_line.GetSwitchValueASCII(kSwitch2));

  // Confirm that there is no '=' in the command line for simple switches.
  std::string switch1_with_equals = std::string("--") +
                                    std::string(kSwitch1) +
                                    std::string("=");
#if defined(OS_WIN)
  EXPECT_EQ(std::wstring::npos,
            command_line.GetCommandLineString().find(
                ASCIIToWide(switch1_with_equals)));
#else
  EXPECT_EQ(std::string::npos,
            command_line.GetCommandLineString().find(switch1_with_equals));
#endif

  // And confirm there is a '=' for switches with values.
  std::string switch2_with_equals = std::string("--") +
                                    std::string(kSwitch2) +
                                    std::string("=");
#if defined(OS_WIN)
  EXPECT_NE(std::wstring::npos,
            command_line.GetCommandLineString().find(
                ASCIIToWide(switch2_with_equals)));
#else
  EXPECT_NE(std::string::npos,
            command_line.GetCommandLineString().find(switch2_with_equals));
#endif

  // And it should persist
  scoped_ptr<ListValue> switch_prefs(GetFlagsExperimentsData(&prefs_));
  ASSERT_TRUE(switch_prefs.get());
  EXPECT_EQ(arraysize(kExperiments), switch_prefs->GetSize());
}

// Tests multi-value type experiments.
TEST_F(AboutFlagsTest, MultiValues) {
  const Experiment& experiment = kExperiments[3];
  ASSERT_EQ(kFlags4, experiment.internal_name);

  // Initially, the first "deactivated" option of the multi experiment should
  // be set.
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&prefs_, &command_line);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch2));
  }

  // Enable the 2nd choice of the multi-value.
  SetExperimentEnabled(&prefs_, experiment.NameForChoice(2), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&prefs_, &command_line);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_TRUE(command_line.HasSwitch(kMultiSwitch2));
    EXPECT_EQ(std::string(kValueForMultiSwitch2),
              command_line.GetSwitchValueASCII(kMultiSwitch2));
  }

  // Disable the multi-value experiment.
  SetExperimentEnabled(&prefs_, experiment.NameForChoice(0), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&prefs_, &command_line);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch2));
  }
}

TEST_F(AboutFlagsTest, EnableDisableValues) {
  const Experiment& experiment = kExperiments[4];
  ASSERT_EQ(kFlags5, experiment.internal_name);

  // Nothing selected.
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&prefs_, &command_line);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kSwitch2));
  }

  // "Enable" option selected.
  SetExperimentEnabled(&prefs_, experiment.NameForChoice(1), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&prefs_, &command_line);
    EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kSwitch2));
    EXPECT_EQ(kEnableDisableValue1, command_line.GetSwitchValueASCII(kSwitch1));
  }

  // "Disable" option selected.
  SetExperimentEnabled(&prefs_, experiment.NameForChoice(2), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&prefs_, &command_line);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
    EXPECT_TRUE(command_line.HasSwitch(kSwitch2));
    EXPECT_EQ(kEnableDisableValue2, command_line.GetSwitchValueASCII(kSwitch2));
  }

  // "Default" option selected, same as nothing selected.
  SetExperimentEnabled(&prefs_, experiment.NameForChoice(0), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&prefs_, &command_line);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch2));
  }
}

// Makes sure there are no separators in any of the experiment names.
TEST_F(AboutFlagsTest, NoSeparators) {
  testing::SetExperiments(NULL, 0);
  size_t count;
  const Experiment* experiments = testing::GetExperiments(&count);
    for (size_t i = 0; i < count; ++i) {
    std::string name = experiments->internal_name;
    EXPECT_EQ(std::string::npos, name.find(testing::kMultiSeparator)) << i;
  }
}

}  // namespace about_flags

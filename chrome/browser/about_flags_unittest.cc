// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace {

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

typedef base::HistogramBase::Sample Sample;
typedef std::map<std::string, Sample> SwitchToIdMap;

// This is a helper function to the ReadEnumFromHistogramsXml().
// Extracts single enum (with integer values) from histograms.xml.
// Expects |reader| to point at given enum.
// Returns map { value => label }.
// Returns empty map on error.
std::map<Sample, std::string> ParseEnumFromHistogramsXml(
    const std::string& enum_name,
    XmlReader* reader) {
  int entries_index = -1;

  std::map<Sample, std::string> result;
  bool success = true;

  while (true) {
    const std::string node_name = reader->NodeName();
    if (node_name == "enum" && reader->IsClosingElement())
      break;

    if (node_name == "int") {
      ++entries_index;
      std::string value_str;
      std::string label;
      const bool has_value = reader->NodeAttribute("value", &value_str);
      const bool has_label = reader->NodeAttribute("label", &label);
      if (!has_value) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", label='" << label
                      << "'): No 'value' attribute.";
        success = false;
      }
      if (!has_label) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", value_str='" << value_str
                      << "'): No 'label' attribute.";
        success = false;
      }

      Sample value;
      if (has_value && !base::StringToInt(value_str, &value)) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", label='" << label
                      << "', value_str='" << value_str
                      << "'): 'value' attribute is not integer.";
        success = false;
      }
      if (result.count(value)) {
        ADD_FAILURE() << "Bad " << enum_name << " enum entry (at index "
                      << entries_index << ", label='" << label
                      << "', value_str='" << value_str
                      << "'): duplicate value '" << value_str
                      << "' found in enum. The previous one has label='"
                      << result[value] << "'.";
        success = false;
      }
      if (success) {
        result[value] = label;
      }
    }
    // All enum entries are on the same level, so it is enough to iterate
    // until possible.
    reader->Next();
  }
  return (success ? result : std::map<Sample, std::string>());
}

// Find and read given enum (with integer values) from histograms.xml.
// |enum_name| - enum name.
// |histograms_xml| - must be loaded histograms.xml file.
//
// Returns map { value => label } so that:
//   <int value="9" label="enable-pinch-virtual-viewport"/>
// becomes:
//   { 9 => "enable-pinch-virtual-viewport" }
// Returns empty map on error.
std::map<Sample, std::string> ReadEnumFromHistogramsXml(
    const std::string& enum_name,
    XmlReader* histograms_xml) {
  std::map<Sample, std::string> login_custom_flags;

  // Implement simple depth first search.
  while (true) {
    const std::string node_name = histograms_xml->NodeName();
    if (node_name == "enum") {
      std::string name;
      if (histograms_xml->NodeAttribute("name", &name) && name == enum_name) {
        if (!login_custom_flags.empty()) {
          EXPECT_TRUE(login_custom_flags.empty())
              << "Duplicate enum '" << enum_name << "' found in histograms.xml";
          return std::map<Sample, std::string>();
        }

        const bool got_into_enum = histograms_xml->Read();
        if (got_into_enum) {
          login_custom_flags =
              ParseEnumFromHistogramsXml(enum_name, histograms_xml);
          EXPECT_FALSE(login_custom_flags.empty())
              << "Bad enum '" << enum_name
              << "' found in histograms.xml (format error).";
        } else {
          EXPECT_TRUE(got_into_enum)
              << "Bad enum '" << enum_name
              << "' (looks empty) found in histograms.xml.";
        }
        if (login_custom_flags.empty())
          return std::map<Sample, std::string>();
      }
    }
    // Go deeper if possible (stops at the closing tag of the deepest node).
    if (histograms_xml->Read())
      continue;

    // Try next node on the same level (skips closing tag).
    if (histograms_xml->Next())
      continue;

    // Go up until next node on the same level exists.
    while (histograms_xml->Depth() && !histograms_xml->SkipToElement()) {
    }

    // Reached top. histograms.xml consists of the single top level node
    // 'histogram-configuration', so this is the end.
    if (!histograms_xml->Depth())
      break;
  }
  EXPECT_FALSE(login_custom_flags.empty())
      << "Enum '" << enum_name << "' is not found in histograms.xml.";
  return login_custom_flags;
}

std::string FilePathStringTypeToString(const base::FilePath::StringType& path) {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(path);
#else
  return path;
#endif
}

std::set<std::string> GetAllSwitchesForTesting() {
  std::set<std::string> result;

  size_t num_experiments = 0;
  const about_flags::Experiment* experiments =
      about_flags::testing::GetExperiments(&num_experiments);

  for (size_t i = 0; i < num_experiments; ++i) {
    const about_flags::Experiment& experiment = experiments[i];
    if (experiment.type == about_flags::Experiment::SINGLE_VALUE) {
      result.insert(experiment.command_line_switch);
    } else if (experiment.type == about_flags::Experiment::MULTI_VALUE) {
      for (int j = 0; j < experiment.num_choices; ++j) {
        result.insert(experiment.choices[j].command_line_switch);
      }
    } else {
      DCHECK_EQ(experiment.type, about_flags::Experiment::ENABLE_DISABLE_VALUE);
      result.insert(experiment.command_line_switch);
      result.insert(experiment.disable_command_line_switch);
    }
  }
  return result;
}

}  // anonymous namespace

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
  AboutFlagsTest() : flags_storage_(&prefs_) {
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
  PrefServiceFlagsStorage flags_storage_;
};


TEST_F(AboutFlagsTest, NoChangeNoRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetExperimentEnabled(&flags_storage_, kFlags1, false);
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, ChangeNeedsRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetExperimentEnabled(&flags_storage_, kFlags1, true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, MultiFlagChangeNeedsRestart) {
  const Experiment& experiment = kExperiments[3];
  ASSERT_EQ(kFlags4, experiment.internal_name);
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  // Enable the 2nd choice of the multi-value.
  SetExperimentEnabled(&flags_storage_, experiment.NameForChoice(2), true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
  testing::ClearState();
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  // Enable the default choice now.
  SetExperimentEnabled(&flags_storage_, experiment.NameForChoice(0), true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveOne) {
  // Add two experiments, check they're there.
  SetExperimentEnabled(&flags_storage_, kFlags1, true);
  SetExperimentEnabled(&flags_storage_, kFlags2, true);

  const base::ListValue* experiments_list = prefs_.GetList(
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
  SetExperimentEnabled(&flags_storage_, kFlags2, false);

  experiments_list = prefs_.GetList(prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list != NULL);
  ASSERT_EQ(1u, experiments_list->GetSize());
  ASSERT_TRUE(experiments_list->GetString(0, &s0));
  EXPECT_TRUE(s0 == kFlags1);
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveBoth) {
  // Add two experiments, check the pref exists.
  SetExperimentEnabled(&flags_storage_, kFlags1, true);
  SetExperimentEnabled(&flags_storage_, kFlags2, true);
  const base::ListValue* experiments_list = prefs_.GetList(
      prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list != NULL);

  // Remove both, the pref should have been removed completely.
  SetExperimentEnabled(&flags_storage_, kFlags1, false);
  SetExperimentEnabled(&flags_storage_, kFlags2, false);
  experiments_list = prefs_.GetList(prefs::kEnabledLabsExperiments);
  EXPECT_TRUE(experiments_list == NULL || experiments_list->GetSize() == 0);
}

TEST_F(AboutFlagsTest, ConvertFlagsToSwitches) {
  SetExperimentEnabled(&flags_storage_, kFlags1, true);

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("foo");

  EXPECT_TRUE(command_line.HasSwitch("foo"));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));

  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);

  EXPECT_TRUE(command_line.HasSwitch("foo"));
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
  EXPECT_TRUE(command_line.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_TRUE(command_line.HasSwitch(switches::kFlagSwitchesEnd));

  CommandLine command_line2(CommandLine::NO_PROGRAM);

  ConvertFlagsToSwitches(&flags_storage_, &command_line2, kNoSentinels);

  EXPECT_TRUE(command_line2.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line2.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_FALSE(command_line2.HasSwitch(switches::kFlagSwitchesEnd));
}

CommandLine::StringType CreateSwitch(const std::string& value) {
#if defined(OS_WIN)
  return base::ASCIIToUTF16(value);
#else
  return value;
#endif
}

TEST_F(AboutFlagsTest, CompareSwitchesToCurrentCommandLine) {
  SetExperimentEnabled(&flags_storage_, kFlags1, true);

  const std::string kDoubleDash("--");

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("foo");

  CommandLine new_command_line(CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage_, &new_command_line, kAddSentinels);

  EXPECT_FALSE(AreSwitchesIdenticalToCurrentCommandLine(
      new_command_line, command_line, NULL));
  {
    std::set<CommandLine::StringType> difference;
    EXPECT_FALSE(AreSwitchesIdenticalToCurrentCommandLine(
        new_command_line, command_line, &difference));
    EXPECT_EQ(1U, difference.size());
    EXPECT_EQ(1U, difference.count(CreateSwitch(kDoubleDash + kSwitch1)));
  }

  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);

  EXPECT_TRUE(AreSwitchesIdenticalToCurrentCommandLine(
      new_command_line, command_line, NULL));
  {
    std::set<CommandLine::StringType> difference;
    EXPECT_TRUE(AreSwitchesIdenticalToCurrentCommandLine(
        new_command_line, command_line, &difference));
    EXPECT_TRUE(difference.empty());
  }

  // Now both have flags but different.
  SetExperimentEnabled(&flags_storage_, kFlags1, false);
  SetExperimentEnabled(&flags_storage_, kFlags2, true);

  CommandLine another_command_line(CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage_, &another_command_line, kAddSentinels);

  EXPECT_FALSE(AreSwitchesIdenticalToCurrentCommandLine(
      new_command_line, another_command_line, NULL));
  {
    std::set<CommandLine::StringType> difference;
    EXPECT_FALSE(AreSwitchesIdenticalToCurrentCommandLine(
        new_command_line, another_command_line, &difference));
    EXPECT_EQ(2U, difference.size());
    EXPECT_EQ(1U, difference.count(CreateSwitch(kDoubleDash + kSwitch1)));
    EXPECT_EQ(1U,
              difference.count(CreateSwitch(kDoubleDash + kSwitch2 + "=" +
                                            kValueForSwitch2)));
  }
}

TEST_F(AboutFlagsTest, RemoveFlagSwitches) {
  std::map<std::string, CommandLine::StringType> switch_list;
  switch_list[kSwitch1] = CommandLine::StringType();
  switch_list[switches::kFlagSwitchesBegin] = CommandLine::StringType();
  switch_list[switches::kFlagSwitchesEnd] = CommandLine::StringType();
  switch_list["foo"] = CommandLine::StringType();

  SetExperimentEnabled(&flags_storage_, kFlags1, true);

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
  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
  RemoveFlagsSwitches(&switch_list);

  // Now the about:flags-related switch should have been removed.
  ASSERT_EQ(1u, switch_list.size());
  EXPECT_TRUE(switch_list.find("foo") != switch_list.end());
}

// Tests enabling experiments that aren't supported on the current platform.
TEST_F(AboutFlagsTest, PersistAndPrune) {
  // Enable experiments 1 and 3.
  SetExperimentEnabled(&flags_storage_, kFlags1, true);
  SetExperimentEnabled(&flags_storage_, kFlags3, true);
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch3));

  // Convert the flags to switches. Experiment 3 shouldn't be among the switches
  // as it is not applicable to the current platform.
  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch3));

  // Experiment 3 should show still be persisted in preferences though.
  const base::ListValue* experiments_list =
      prefs_.GetList(prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list);
  EXPECT_EQ(2U, experiments_list->GetSize());
  std::string s0;
  ASSERT_TRUE(experiments_list->GetString(0, &s0));
  EXPECT_EQ(kFlags1, s0);
  std::string s1;
  ASSERT_TRUE(experiments_list->GetString(1, &s1));
  EXPECT_EQ(kFlags3, s1);
}

// Tests that switches which should have values get them in the command
// line.
TEST_F(AboutFlagsTest, CheckValues) {
  // Enable experiments 1 and 2.
  SetExperimentEnabled(&flags_storage_, kFlags1, true);
  SetExperimentEnabled(&flags_storage_, kFlags2, true);
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch2));

  // Convert the flags to switches.
  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
  EXPECT_EQ(std::string(), command_line.GetSwitchValueASCII(kSwitch1));
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
                base::ASCIIToWide(switch1_with_equals)));
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
                base::ASCIIToWide(switch2_with_equals)));
#else
  EXPECT_NE(std::string::npos,
            command_line.GetCommandLineString().find(switch2_with_equals));
#endif

  // And it should persist.
  const base::ListValue* experiments_list =
      prefs_.GetList(prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(experiments_list);
  EXPECT_EQ(2U, experiments_list->GetSize());
  std::string s0;
  ASSERT_TRUE(experiments_list->GetString(0, &s0));
  EXPECT_EQ(kFlags1, s0);
  std::string s1;
  ASSERT_TRUE(experiments_list->GetString(1, &s1));
  EXPECT_EQ(kFlags2, s1);
}

// Tests multi-value type experiments.
TEST_F(AboutFlagsTest, MultiValues) {
  const Experiment& experiment = kExperiments[3];
  ASSERT_EQ(kFlags4, experiment.internal_name);

  // Initially, the first "deactivated" option of the multi experiment should
  // be set.
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch2));
  }

  // Enable the 2nd choice of the multi-value.
  SetExperimentEnabled(&flags_storage_, experiment.NameForChoice(2), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_TRUE(command_line.HasSwitch(kMultiSwitch2));
    EXPECT_EQ(std::string(kValueForMultiSwitch2),
              command_line.GetSwitchValueASCII(kMultiSwitch2));
  }

  // Disable the multi-value experiment.
  SetExperimentEnabled(&flags_storage_, experiment.NameForChoice(0), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
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
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kSwitch2));
  }

  // "Enable" option selected.
  SetExperimentEnabled(&flags_storage_, experiment.NameForChoice(1), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kSwitch2));
    EXPECT_EQ(kEnableDisableValue1, command_line.GetSwitchValueASCII(kSwitch1));
  }

  // "Disable" option selected.
  SetExperimentEnabled(&flags_storage_, experiment.NameForChoice(2), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
    EXPECT_TRUE(command_line.HasSwitch(kSwitch2));
    EXPECT_EQ(kEnableDisableValue2, command_line.GetSwitchValueASCII(kSwitch2));
  }

  // "Default" option selected, same as nothing selected.
  SetExperimentEnabled(&flags_storage_, experiment.NameForChoice(0), true);
  {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
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

class AboutFlagsHistogramTest : public ::testing::Test {
 protected:
  // This is a helper function to check that all IDs in enum LoginCustomFlags in
  // histograms.xml are unique.
  void SetSwitchToHistogramIdMapping(const std::string& switch_name,
                                     const Sample switch_histogram_id,
                                     std::map<std::string, Sample>* out_map) {
    const std::pair<std::map<std::string, Sample>::iterator, bool> status =
        out_map->insert(std::make_pair(switch_name, switch_histogram_id));
    if (!status.second) {
      EXPECT_TRUE(status.first->second == switch_histogram_id)
          << "Duplicate switch '" << switch_name
          << "' found in enum 'LoginCustomFlags' in histograms.xml.";
    }
  }

  // This method generates a hint for the user for what string should be added
  // to the enum LoginCustomFlags to make in consistent.
  std::string GetHistogramEnumEntryText(const std::string& switch_name,
                                        Sample value) {
    return base::StringPrintf(
        "<int value=\"%d\" label=\"%s\"/>", value, switch_name.c_str());
  }
};

TEST_F(AboutFlagsHistogramTest, CheckHistograms) {
  base::FilePath histograms_xml_file_path;
  ASSERT_TRUE(
      PathService::Get(base::DIR_SOURCE_ROOT, &histograms_xml_file_path));
  histograms_xml_file_path = histograms_xml_file_path.AppendASCII("tools")
      .AppendASCII("metrics")
      .AppendASCII("histograms")
      .AppendASCII("histograms.xml");

  XmlReader histograms_xml;
  ASSERT_TRUE(histograms_xml.LoadFile(
      FilePathStringTypeToString(histograms_xml_file_path.value())));
  std::map<Sample, std::string> login_custom_flags =
      ReadEnumFromHistogramsXml("LoginCustomFlags", &histograms_xml);
  ASSERT_TRUE(login_custom_flags.size())
      << "Error reading enum 'LoginCustomFlags' from histograms.xml.";

  // Build reverse map {switch_name => id} from login_custom_flags.
  SwitchToIdMap histograms_xml_switches_ids;

  EXPECT_TRUE(login_custom_flags.count(kBadSwitchFormatHistogramId))
      << "Entry for UMA ID of incorrect command-line flag is not found in "
         "histograms.xml enum LoginCustomFlags. "
         "Consider adding entry:\n"
      << "  " << GetHistogramEnumEntryText("BAD_FLAG_FORMAT", 0);
  // Check that all LoginCustomFlags entries have correct values.
  for (std::map<Sample, std::string>::const_iterator it =
           login_custom_flags.begin();
       it != login_custom_flags.end();
       ++it) {
    if (it->first == kBadSwitchFormatHistogramId) {
      // Add eror value with empty name.
      SetSwitchToHistogramIdMapping(
          "", it->first, &histograms_xml_switches_ids);
      continue;
    }
    const Sample uma_id = GetSwitchUMAId(it->second);
    EXPECT_EQ(uma_id, it->first)
        << "histograms.xml enum LoginCustomFlags "
           "entry '" << it->second << "' has incorrect value=" << it->first
        << ", but " << uma_id << " is expected. Consider changing entry to:\n"
        << "  " << GetHistogramEnumEntryText(it->second, uma_id);
    SetSwitchToHistogramIdMapping(
        it->second, it->first, &histograms_xml_switches_ids);
  }

  // Check that all flags in about_flags.cc have entries in login_custom_flags.
  std::set<std::string> all_switches = GetAllSwitchesForTesting();
  for (std::set<std::string>::const_iterator it = all_switches.begin();
       it != all_switches.end();
       ++it) {
    // Skip empty placeholders.
    if (it->empty())
      continue;
    const Sample uma_id = GetSwitchUMAId(*it);
    EXPECT_NE(kBadSwitchFormatHistogramId, uma_id)
        << "Command-line switch '" << *it
        << "' from about_flags.cc has UMA ID equal to reserved value "
           "kBadSwitchFormatHistogramId=" << kBadSwitchFormatHistogramId
        << ". Please modify switch name.";
    SwitchToIdMap::iterator enum_entry =
        histograms_xml_switches_ids.lower_bound(*it);

    // Ignore case here when switch ID is incorrect - it has already been
    // reported in the previous loop.
    EXPECT_TRUE(enum_entry != histograms_xml_switches_ids.end() &&
                enum_entry->first == *it)
        << "histograms.xml enum LoginCustomFlags doesn't contain switch '"
        << *it << "' (value=" << uma_id
        << " expected). Consider adding entry:\n"
        << "  " << GetHistogramEnumEntryText(*it, uma_id);
  }
}

}  // namespace about_flags

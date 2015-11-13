// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <stdint.h>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace about_flags {

namespace {

const char kFlags1[] = "flag1";
const char kFlags2[] = "flag2";
const char kFlags3[] = "flag3";
const char kFlags4[] = "flag4";
const char kFlags5[] = "flag5";
const char kFlags6[] = "flag6";
const char kFlags7[] = "flag7";

const char kSwitch1[] = "switch";
const char kSwitch2[] = "switch2";
const char kSwitch3[] = "switch3";
const char kSwitch6[] = "switch6";
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

// Get all associated switches corresponding to defined about_flags.cc entries.
// Does not include information about FEATURE_VALUE entries.
std::set<std::string> GetAllSwitchesForTesting() {
  std::set<std::string> result;

  size_t num_entries = 0;
  const FeatureEntry* entries =
      testing::GetFeatureEntries(&num_entries);

  for (size_t i = 0; i < num_entries; ++i) {
    const FeatureEntry& entry = entries[i];
    switch (entry.type) {
      case FeatureEntry::SINGLE_VALUE:
      case FeatureEntry::SINGLE_DISABLE_VALUE:
        result.insert(entry.command_line_switch);
        break;
      case FeatureEntry::MULTI_VALUE:
        for (int j = 0; j < entry.num_choices; ++j) {
          result.insert(entry.choices[j].command_line_switch);
        }
        break;
      case FeatureEntry::ENABLE_DISABLE_VALUE:
        result.insert(entry.command_line_switch);
        result.insert(entry.disable_command_line_switch);
        break;
      case FeatureEntry::FEATURE_VALUE:
        break;
    }
  }
  return result;
}

}  // anonymous namespace

const FeatureEntry::Choice kMultiChoices[] = {
  { IDS_PRODUCT_NAME, "", "" },
  { IDS_PRODUCT_NAME, kMultiSwitch1, "" },
  { IDS_PRODUCT_NAME, kMultiSwitch2, kValueForMultiSwitch2 },
};

const base::Feature kTestFeature{"FeatureName",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

// The entries that are set for these tests. The 3rd entry is not supported on
// the current platform, all others are.
static FeatureEntry kEntries[] = {
    {kFlags1, IDS_PRODUCT_NAME, IDS_PRODUCT_NAME,
     0,  // Ends up being mapped to the current platform.
     FeatureEntry::SINGLE_VALUE, kSwitch1, "", nullptr, nullptr, nullptr,
     nullptr, 0},
    {kFlags2, IDS_PRODUCT_NAME, IDS_PRODUCT_NAME,
     0,  // Ends up being mapped to the current platform.
     FeatureEntry::SINGLE_VALUE, kSwitch2, kValueForSwitch2, nullptr, nullptr,
     nullptr, nullptr, 0},
    {kFlags3, IDS_PRODUCT_NAME, IDS_PRODUCT_NAME,
     0,  // This ends up enabling for an OS other than the current.
     FeatureEntry::SINGLE_VALUE, kSwitch3, "", nullptr, nullptr, nullptr,
     nullptr, 0},
    {kFlags4, IDS_PRODUCT_NAME, IDS_PRODUCT_NAME,
     0,  // Ends up being mapped to the current platform.
     FeatureEntry::MULTI_VALUE, "", "", "", "", nullptr, kMultiChoices,
     arraysize(kMultiChoices)},
    {kFlags5, IDS_PRODUCT_NAME, IDS_PRODUCT_NAME,
     0,  // Ends up being mapped to the current platform.
     FeatureEntry::ENABLE_DISABLE_VALUE, kSwitch1, kEnableDisableValue1,
     kSwitch2, kEnableDisableValue2, nullptr, nullptr, 3},
    {kFlags6, IDS_PRODUCT_NAME, IDS_PRODUCT_NAME, 0,
     FeatureEntry::SINGLE_DISABLE_VALUE, kSwitch6, "", nullptr, nullptr,
     nullptr, nullptr, 0},
    {kFlags7, IDS_PRODUCT_NAME, IDS_PRODUCT_NAME,
     0,  // Ends up being mapped to the current platform.
     FeatureEntry::FEATURE_VALUE, nullptr, nullptr, nullptr, nullptr,
     &kTestFeature, nullptr, 3},
};

class AboutFlagsTest : public ::testing::Test {
 protected:
  AboutFlagsTest() : flags_storage_(&prefs_) {
    prefs_.registry()->RegisterListPref(
        flags_ui::prefs::kEnabledLabsExperiments);
    testing::ClearState();
  }

  void SetUp() override {
    for (size_t i = 0; i < arraysize(kEntries); ++i)
      kEntries[i].supported_platforms = GetCurrentPlatform();

    int os_other_than_current = 1;
    while (os_other_than_current == GetCurrentPlatform())
      os_other_than_current <<= 1;
    kEntries[2].supported_platforms = os_other_than_current;

    testing::SetFeatureEntries(kEntries, arraysize(kEntries));
  }

  void TearDown() override { testing::SetFeatureEntries(nullptr, 0); }

  TestingPrefServiceSimple prefs_;
  flags_ui::PrefServiceFlagsStorage flags_storage_;
};


TEST_F(AboutFlagsTest, NoChangeNoRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, false);
  EXPECT_FALSE(IsRestartNeededToCommitChanges());

  // kFlags6 is enabled by default, so enabling should not require a restart.
  SetFeatureEntryEnabled(&flags_storage_, kFlags6, true);
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, ChangeNeedsRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

// Tests that disabling a default enabled entry requires a restart.
TEST_F(AboutFlagsTest, DisableChangeNeedsRestart) {
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  SetFeatureEntryEnabled(&flags_storage_, kFlags6, false);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, MultiFlagChangeNeedsRestart) {
  const FeatureEntry& entry = kEntries[3];
  ASSERT_EQ(kFlags4, entry.internal_name);
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  // Enable the 2nd choice of the multi-value.
  SetFeatureEntryEnabled(&flags_storage_, entry.NameForChoice(2), true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
  testing::ClearState();
  EXPECT_FALSE(IsRestartNeededToCommitChanges());
  // Enable the default choice now.
  SetFeatureEntryEnabled(&flags_storage_, entry.NameForChoice(0), true);
  EXPECT_TRUE(IsRestartNeededToCommitChanges());
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveOne) {
  // Add two entries, check they're there.
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);
  SetFeatureEntryEnabled(&flags_storage_, kFlags2, true);

  const base::ListValue* entries_list =
      prefs_.GetList(flags_ui::prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(entries_list != nullptr);

  ASSERT_EQ(2u, entries_list->GetSize());

  std::string s0;
  ASSERT_TRUE(entries_list->GetString(0, &s0));
  std::string s1;
  ASSERT_TRUE(entries_list->GetString(1, &s1));

  EXPECT_TRUE(s0 == kFlags1 || s1 == kFlags1);
  EXPECT_TRUE(s0 == kFlags2 || s1 == kFlags2);

  // Remove one entry, check the other's still around.
  SetFeatureEntryEnabled(&flags_storage_, kFlags2, false);

  entries_list = prefs_.GetList(flags_ui::prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(entries_list != nullptr);
  ASSERT_EQ(1u, entries_list->GetSize());
  ASSERT_TRUE(entries_list->GetString(0, &s0));
  EXPECT_TRUE(s0 == kFlags1);
}

TEST_F(AboutFlagsTest, AddTwoFlagsRemoveBoth) {
  // Add two entries, check the pref exists.
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);
  SetFeatureEntryEnabled(&flags_storage_, kFlags2, true);
  const base::ListValue* entries_list =
      prefs_.GetList(flags_ui::prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(entries_list != nullptr);

  // Remove both, the pref should have been removed completely.
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, false);
  SetFeatureEntryEnabled(&flags_storage_, kFlags2, false);
  entries_list = prefs_.GetList(flags_ui::prefs::kEnabledLabsExperiments);
  EXPECT_TRUE(entries_list == nullptr || entries_list->GetSize() == 0);
}

TEST_F(AboutFlagsTest, ConvertFlagsToSwitches) {
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("foo");

  EXPECT_TRUE(command_line.HasSwitch("foo"));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));

  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);

  EXPECT_TRUE(command_line.HasSwitch("foo"));
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
  EXPECT_TRUE(command_line.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_TRUE(command_line.HasSwitch(switches::kFlagSwitchesEnd));

  base::CommandLine command_line2(base::CommandLine::NO_PROGRAM);

  ConvertFlagsToSwitches(&flags_storage_, &command_line2, kNoSentinels);

  EXPECT_TRUE(command_line2.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line2.HasSwitch(switches::kFlagSwitchesBegin));
  EXPECT_FALSE(command_line2.HasSwitch(switches::kFlagSwitchesEnd));
}

base::CommandLine::StringType CreateSwitch(const std::string& value) {
#if defined(OS_WIN)
  return base::ASCIIToUTF16(value);
#else
  return value;
#endif
}

TEST_F(AboutFlagsTest, CompareSwitchesToCurrentCommandLine) {
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);

  const std::string kDoubleDash("--");

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("foo");

  base::CommandLine new_command_line(base::CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage_, &new_command_line, kAddSentinels);

  EXPECT_FALSE(AreSwitchesIdenticalToCurrentCommandLine(new_command_line,
                                                        command_line, nullptr));
  {
    std::set<base::CommandLine::StringType> difference;
    EXPECT_FALSE(AreSwitchesIdenticalToCurrentCommandLine(
        new_command_line, command_line, &difference));
    EXPECT_EQ(1U, difference.size());
    EXPECT_EQ(1U, difference.count(CreateSwitch(kDoubleDash + kSwitch1)));
  }

  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);

  EXPECT_TRUE(AreSwitchesIdenticalToCurrentCommandLine(new_command_line,
                                                       command_line, nullptr));
  {
    std::set<base::CommandLine::StringType> difference;
    EXPECT_TRUE(AreSwitchesIdenticalToCurrentCommandLine(
        new_command_line, command_line, &difference));
    EXPECT_TRUE(difference.empty());
  }

  // Now both have flags but different.
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, false);
  SetFeatureEntryEnabled(&flags_storage_, kFlags2, true);

  base::CommandLine another_command_line(base::CommandLine::NO_PROGRAM);
  ConvertFlagsToSwitches(&flags_storage_, &another_command_line, kAddSentinels);

  EXPECT_FALSE(AreSwitchesIdenticalToCurrentCommandLine(
      new_command_line, another_command_line, nullptr));
  {
    std::set<base::CommandLine::StringType> difference;
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
  std::map<std::string, base::CommandLine::StringType> switch_list;
  switch_list[kSwitch1] = base::CommandLine::StringType();
  switch_list[switches::kFlagSwitchesBegin] = base::CommandLine::StringType();
  switch_list[switches::kFlagSwitchesEnd] = base::CommandLine::StringType();
  switch_list["foo"] = base::CommandLine::StringType();

  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);

  // This shouldn't do anything before ConvertFlagsToSwitches() wasn't called.
  RemoveFlagsSwitches(&switch_list);
  ASSERT_EQ(4u, switch_list.size());
  EXPECT_TRUE(ContainsKey(switch_list, kSwitch1));
  EXPECT_TRUE(ContainsKey(switch_list, switches::kFlagSwitchesBegin));
  EXPECT_TRUE(ContainsKey(switch_list, switches::kFlagSwitchesEnd));
  EXPECT_TRUE(ContainsKey(switch_list, "foo"));

  // Call ConvertFlagsToSwitches(), then RemoveFlagsSwitches() again.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("foo");
  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
  RemoveFlagsSwitches(&switch_list);

  // Now the about:flags-related switch should have been removed.
  ASSERT_EQ(1u, switch_list.size());
  EXPECT_TRUE(ContainsKey(switch_list, "foo"));
}

TEST_F(AboutFlagsTest, RemoveFlagSwitches_Features) {
  struct {
    int enabled_choice;  // 0: default, 1: enabled, 2: disabled.
    const char* existing_enable_features;
    const char* existing_disable_features;
    const char* expected_enable_features;
    const char* expected_disable_features;
  } cases[] = {
      // Default value: Should not affect existing flags.
      {0, nullptr, nullptr, nullptr, nullptr},
      {0, "A,B", "C", "A,B", "C"},
      // "Enable" option: should only affect enabled list.
      {1, nullptr, nullptr, "FeatureName", nullptr},
      {1, "A,B", "C", "A,B,FeatureName", "C"},
      // "Disable" option: should only affect disabled list.
      {2, nullptr, nullptr, nullptr, "FeatureName"},
      {2, "A,B", "C", "A,B", "C,FeatureName"},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "Test[%" PRIuS "]: %d [%s] [%s]", i, cases[i].enabled_choice,
        cases[i].existing_enable_features ? cases[i].existing_enable_features
                                          : "null",
        cases[i].existing_disable_features ? cases[i].existing_disable_features
                                           : "null"));

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (cases[i].existing_enable_features) {
      command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                     cases[i].existing_enable_features);
    }
    if (cases[i].existing_disable_features) {
      command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                     cases[i].existing_disable_features);
    }

    testing::ClearState();

    const std::string entry_name = base::StringPrintf(
        "%s%s%d", kFlags7, testing::kMultiSeparator, cases[i].enabled_choice);
    SetFeatureEntryEnabled(&flags_storage_, entry_name, true);

    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    auto switch_list = command_line.GetSwitches();
    EXPECT_EQ(cases[i].expected_enable_features != nullptr,
              ContainsKey(switch_list, switches::kEnableFeatures));
    if (cases[i].expected_enable_features)
      EXPECT_EQ(CreateSwitch(cases[i].expected_enable_features),
                switch_list[switches::kEnableFeatures]);

    EXPECT_EQ(cases[i].expected_disable_features != nullptr,
              ContainsKey(switch_list, switches::kDisableFeatures));
    if (cases[i].expected_disable_features)
      EXPECT_EQ(CreateSwitch(cases[i].expected_disable_features),
                switch_list[switches::kDisableFeatures]);

    // RemoveFlagsSwitches() should result in the original values for these
    // switches.
    switch_list = command_line.GetSwitches();
    RemoveFlagsSwitches(&switch_list);
    EXPECT_EQ(cases[i].existing_enable_features != nullptr,
              ContainsKey(switch_list, switches::kEnableFeatures));
    if (cases[i].existing_enable_features)
      EXPECT_EQ(CreateSwitch(cases[i].existing_enable_features),
                switch_list[switches::kEnableFeatures]);
    EXPECT_EQ(cases[i].existing_disable_features != nullptr,
              ContainsKey(switch_list, switches::kEnableFeatures));
    if (cases[i].existing_disable_features)
      EXPECT_EQ(CreateSwitch(cases[i].existing_disable_features),
                switch_list[switches::kDisableFeatures]);
  }
}

// Tests enabling entries that aren't supported on the current platform.
TEST_F(AboutFlagsTest, PersistAndPrune) {
  // Enable entries 1 and 3.
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);
  SetFeatureEntryEnabled(&flags_storage_, kFlags3, true);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch3));

  // Convert the flags to switches. Entry 3 shouldn't be among the switches
  // as it is not applicable to the current platform.
  ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
  EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
  EXPECT_FALSE(command_line.HasSwitch(kSwitch3));

  // FeatureEntry 3 should show still be persisted in preferences though.
  const base::ListValue* entries_list =
      prefs_.GetList(flags_ui::prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(entries_list);
  EXPECT_EQ(2U, entries_list->GetSize());
  std::string s0;
  ASSERT_TRUE(entries_list->GetString(0, &s0));
  EXPECT_EQ(kFlags1, s0);
  std::string s1;
  ASSERT_TRUE(entries_list->GetString(1, &s1));
  EXPECT_EQ(kFlags3, s1);
}

// Tests that switches which should have values get them in the command
// line.
TEST_F(AboutFlagsTest, CheckValues) {
  // Enable entries 1 and 2.
  SetFeatureEntryEnabled(&flags_storage_, kFlags1, true);
  SetFeatureEntryEnabled(&flags_storage_, kFlags2, true);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
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
  EXPECT_EQ(base::string16::npos,
            command_line.GetCommandLineString().find(
                base::ASCIIToUTF16(switch1_with_equals)));
#else
  EXPECT_EQ(std::string::npos,
            command_line.GetCommandLineString().find(switch1_with_equals));
#endif

  // And confirm there is a '=' for switches with values.
  std::string switch2_with_equals = std::string("--") +
                                    std::string(kSwitch2) +
                                    std::string("=");
#if defined(OS_WIN)
  EXPECT_NE(base::string16::npos,
            command_line.GetCommandLineString().find(
                base::ASCIIToUTF16(switch2_with_equals)));
#else
  EXPECT_NE(std::string::npos,
            command_line.GetCommandLineString().find(switch2_with_equals));
#endif

  // And it should persist.
  const base::ListValue* entries_list =
      prefs_.GetList(flags_ui::prefs::kEnabledLabsExperiments);
  ASSERT_TRUE(entries_list);
  EXPECT_EQ(2U, entries_list->GetSize());
  std::string s0;
  ASSERT_TRUE(entries_list->GetString(0, &s0));
  EXPECT_EQ(kFlags1, s0);
  std::string s1;
  ASSERT_TRUE(entries_list->GetString(1, &s1));
  EXPECT_EQ(kFlags2, s1);
}

// Tests multi-value type entries.
TEST_F(AboutFlagsTest, MultiValues) {
  const FeatureEntry& entry = kEntries[3];
  ASSERT_EQ(kFlags4, entry.internal_name);

  // Initially, the first "deactivated" option of the multi entry should
  // be set.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch2));
  }

  // Enable the 2nd choice of the multi-value.
  SetFeatureEntryEnabled(&flags_storage_, entry.NameForChoice(2), true);
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_TRUE(command_line.HasSwitch(kMultiSwitch2));
    EXPECT_EQ(std::string(kValueForMultiSwitch2),
              command_line.GetSwitchValueASCII(kMultiSwitch2));
  }

  // Disable the multi-value entry.
  SetFeatureEntryEnabled(&flags_storage_, entry.NameForChoice(0), true);
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch2));
  }
}

// Tests that disable flags are added when an entry is disabled.
TEST_F(AboutFlagsTest, DisableFlagCommandLine) {
  // Nothing selected.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch6));
  }

  // Disable the entry 6.
  SetFeatureEntryEnabled(&flags_storage_, kFlags6, false);
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_TRUE(command_line.HasSwitch(kSwitch6));
  }

  // Enable entry 6.
  SetFeatureEntryEnabled(&flags_storage_, kFlags6, true);
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch6));
  }
}

TEST_F(AboutFlagsTest, EnableDisableValues) {
  const FeatureEntry& entry = kEntries[4];
  ASSERT_EQ(kFlags5, entry.internal_name);

  // Nothing selected.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kSwitch2));
  }

  // "Enable" option selected.
  SetFeatureEntryEnabled(&flags_storage_, entry.NameForChoice(1), true);
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_TRUE(command_line.HasSwitch(kSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kSwitch2));
    EXPECT_EQ(kEnableDisableValue1, command_line.GetSwitchValueASCII(kSwitch1));
  }

  // "Disable" option selected.
  SetFeatureEntryEnabled(&flags_storage_, entry.NameForChoice(2), true);
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kSwitch1));
    EXPECT_TRUE(command_line.HasSwitch(kSwitch2));
    EXPECT_EQ(kEnableDisableValue2, command_line.GetSwitchValueASCII(kSwitch2));
  }

  // "Default" option selected, same as nothing selected.
  SetFeatureEntryEnabled(&flags_storage_, entry.NameForChoice(0), true);
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch1));
    EXPECT_FALSE(command_line.HasSwitch(kMultiSwitch2));
  }
}

TEST_F(AboutFlagsTest, FeatureValues) {
  const FeatureEntry& entry = kEntries[6];
  ASSERT_EQ(kFlags7, entry.internal_name);

  struct {
    int enabled_choice;
    const char* existing_enable_features;
    const char* existing_disable_features;
    const char* expected_enable_features;
    const char* expected_disable_features;
  } cases[] = {
      // Nothing selected.
      {-1, nullptr, nullptr, "", ""},
      // "Default" option selected, same as nothing selected.
      {0, nullptr, nullptr, "", ""},
      // "Enable" option selected.
      {1, nullptr, nullptr, "FeatureName", ""},
      // "Disable" option selected.
      {2, nullptr, nullptr, "", "FeatureName"},
      // "Enable" option should get added to the existing list.
      {1, "Foo,Bar", nullptr, "Foo,Bar,FeatureName", ""},
      // "Disable" option should get added to the existing list.
      {2, nullptr, "Foo,Bar", "", "Foo,Bar,FeatureName"},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "Test[%" PRIuS "]: %d [%s] [%s]", i, cases[i].enabled_choice,
        cases[i].existing_enable_features ? cases[i].existing_enable_features
                                          : "null",
        cases[i].existing_disable_features ? cases[i].existing_disable_features
                                           : "null"));

    if (cases[i].enabled_choice != -1) {
      SetFeatureEntryEnabled(
          &flags_storage_, entry.NameForChoice(cases[i].enabled_choice), true);
    }

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (cases[i].existing_enable_features) {
      command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                     cases[i].existing_enable_features);
    }
    if (cases[i].existing_disable_features) {
      command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                     cases[i].existing_disable_features);
    }

    ConvertFlagsToSwitches(&flags_storage_, &command_line, kAddSentinels);
    EXPECT_EQ(cases[i].expected_enable_features,
              command_line.GetSwitchValueASCII(switches::kEnableFeatures));
    EXPECT_EQ(cases[i].expected_disable_features,
              command_line.GetSwitchValueASCII(switches::kDisableFeatures));
  }
}

// Makes sure there are no separators in any of the entry names.
TEST_F(AboutFlagsTest, NoSeparators) {
  testing::SetFeatureEntries(nullptr, 0);
  size_t count;
  const FeatureEntry* entries = testing::GetFeatureEntries(&count);
  for (size_t i = 0; i < count; ++i) {
    std::string name = entries[i].internal_name;
    EXPECT_EQ(std::string::npos, name.find(testing::kMultiSeparator)) << i;
  }
}

TEST_F(AboutFlagsTest, GetFlagFeatureEntries) {
  base::ListValue supported_entries;
  base::ListValue unsupported_entries;
  GetFlagFeatureEntries(&flags_storage_, kGeneralAccessFlagsOnly,
                        &supported_entries, &unsupported_entries);
  // All |kEntries| except for |kFlags3| should be supported.
  EXPECT_EQ(6u, supported_entries.GetSize());
  EXPECT_EQ(1u, unsupported_entries.GetSize());
  EXPECT_EQ(arraysize(kEntries),
            supported_entries.GetSize() + unsupported_entries.GetSize());
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

  EXPECT_TRUE(login_custom_flags.count(testing::kBadSwitchFormatHistogramId))
      << "Entry for UMA ID of incorrect command-line flag is not found in "
         "histograms.xml enum LoginCustomFlags. "
         "Consider adding entry:\n"
      << "  " << GetHistogramEnumEntryText("BAD_FLAG_FORMAT", 0);
  // Check that all LoginCustomFlags entries have correct values.
  for (const auto& entry : login_custom_flags) {
    if (entry.first == testing::kBadSwitchFormatHistogramId) {
      // Add error value with empty name.
      SetSwitchToHistogramIdMapping(std::string(), entry.first,
                                    &histograms_xml_switches_ids);
      continue;
    }
    const Sample uma_id = GetSwitchUMAId(entry.second);
    EXPECT_EQ(uma_id, entry.first)
        << "histograms.xml enum LoginCustomFlags "
           "entry '" << entry.second << "' has incorrect value=" << entry.first
        << ", but " << uma_id << " is expected. Consider changing entry to:\n"
        << "  " << GetHistogramEnumEntryText(entry.second, uma_id);
    SetSwitchToHistogramIdMapping(entry.second, entry.first,
                                  &histograms_xml_switches_ids);
  }

  // Check that all flags in about_flags.cc have entries in login_custom_flags.
  std::set<std::string> all_switches = GetAllSwitchesForTesting();
  for (const std::string& flag : all_switches) {
    // Skip empty placeholders.
    if (flag.empty())
      continue;
    const Sample uma_id = GetSwitchUMAId(flag);
    EXPECT_NE(testing::kBadSwitchFormatHistogramId, uma_id)
        << "Command-line switch '" << flag
        << "' from about_flags.cc has UMA ID equal to reserved value "
           "kBadSwitchFormatHistogramId="
        << testing::kBadSwitchFormatHistogramId
        << ". Please modify switch name.";
    SwitchToIdMap::iterator enum_entry =
        histograms_xml_switches_ids.lower_bound(flag);

    // Ignore case here when switch ID is incorrect - it has already been
    // reported in the previous loop.
    EXPECT_TRUE(enum_entry != histograms_xml_switches_ids.end() &&
                enum_entry->first == flag)
        << "histograms.xml enum LoginCustomFlags doesn't contain switch '"
        << flag << "' (value=" << uma_id
        << " expected). Consider adding entry:\n"
        << "  " << GetHistogramEnumEntryText(flag, uma_id);
  }
}

}  // namespace about_flags

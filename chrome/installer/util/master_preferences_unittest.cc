// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for master preferences related methods.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MasterPreferencesTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(file_util::CreateTemporaryFile(&prefs_file_));
  }

  virtual void TearDown() {
    EXPECT_TRUE(file_util::Delete(prefs_file_, false));
  }

  const FilePath& prefs_file() const { return prefs_file_; }

 private:
  FilePath prefs_file_;
};

// Used to specify an expected value for a set boolean preference variable.
struct ExpectedBooleans {
  const char* name;
  bool expected_value;
};

}  // namespace

TEST_F(MasterPreferencesTest, ParseDistroParams) {
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"skip_first_run_ui\": true,\n"
    "     \"show_welcome_page\": true,\n"
    "     \"import_search_engine\": true,\n"
    "     \"import_history\": true,\n"
    "     \"import_bookmarks\": true,\n"
    "     \"import_bookmarks_from_file\": \"c:\\\\foo\",\n"
    "     \"import_home_page\": true,\n"
    "     \"create_all_shortcuts\": true,\n"
    "     \"do_not_launch_chrome\": true,\n"
    "     \"make_chrome_default\": true,\n"
    "     \"make_chrome_default_for_user\": true,\n"
    "     \"system_level\": true,\n"
    "     \"verbose_logging\": true,\n"
    "     \"require_eula\": true,\n"
    "     \"alternate_shortcut_text\": true,\n"
    "     \"oem_bubble\": true,\n"
    "     \"chrome_shortcut_icon_index\": 1,\n"
    "     \"ping_delay\": 40,\n"
    "     \"search_engine_experiment\": true\n"
    "  },\n"
    "  \"blah\": {\n"
    "     \"import_history\": false\n"
    "  }\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs_file(), text, sizeof(text)));
  installer_util::MasterPreferences prefs(prefs_file());

  const char* expected_true[] = {
    installer_util::master_preferences::kDistroSkipFirstRunPref,
    installer_util::master_preferences::kDistroShowWelcomePage,
    installer_util::master_preferences::kDistroImportSearchPref,
    installer_util::master_preferences::kDistroImportHistoryPref,
    installer_util::master_preferences::kDistroImportBookmarksPref,
    installer_util::master_preferences::kDistroImportHomePagePref,
    installer_util::master_preferences::kCreateAllShortcuts,
    installer_util::master_preferences::kDoNotLaunchChrome,
    installer_util::master_preferences::kMakeChromeDefault,
    installer_util::master_preferences::kMakeChromeDefaultForUser,
    installer_util::master_preferences::kSystemLevel,
    installer_util::master_preferences::kVerboseLogging,
    installer_util::master_preferences::kRequireEula,
    installer_util::master_preferences::kAltShortcutText,
    installer_util::master_preferences::kAltFirstRunBubble,
    installer_util::master_preferences::kSearchEngineExperimentPref,
  };

  for (int i = 0; i < arraysize(expected_true); ++i) {
    bool value = false;
    EXPECT_TRUE(prefs.GetBool(expected_true[i], &value));
    EXPECT_TRUE(value) << expected_true[i];
  }

  std::string str_value;
  EXPECT_TRUE(prefs.GetString(
      installer_util::master_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));
  EXPECT_STREQ("c:\\foo", str_value.c_str());

  int icon_index = 0;
  EXPECT_TRUE(prefs.GetInt(
      installer_util::master_preferences::kChromeShortcutIconIndex,
      &icon_index));
  EXPECT_EQ(icon_index, 1);
  int ping_delay = 90;
  EXPECT_TRUE(prefs.GetInt(installer_util::master_preferences::kDistroPingDelay,
                           &ping_delay));
  EXPECT_EQ(ping_delay, 40);
}

TEST_F(MasterPreferencesTest, ParseMissingDistroParams) {
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"skip_first_run_ui\": true,\n"
    "     \"import_search_engine\": true,\n"
    "     \"import_bookmarks\": false,\n"
    "     \"import_bookmarks_from_file\": \"\",\n"
    "     \"create_all_shortcuts\": true,\n"
    "     \"do_not_launch_chrome\": true,\n"
    "     \"chrome_shortcut_icon_index\": \"bac\"\n"
    "  }\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs_file(), text, sizeof(text)));
  installer_util::MasterPreferences prefs(prefs_file());

  ExpectedBooleans expected_bool[] = {
    { installer_util::master_preferences::kDistroSkipFirstRunPref, true },
    { installer_util::master_preferences::kDistroImportSearchPref, true },
    { installer_util::master_preferences::kDistroImportBookmarksPref, false },
    { installer_util::master_preferences::kCreateAllShortcuts, true },
    { installer_util::master_preferences::kDoNotLaunchChrome, true },
  };

  bool value = false;
  for (int i = 0; i < arraysize(expected_bool); ++i) {
    EXPECT_TRUE(prefs.GetBool(expected_bool[i].name, &value));
    EXPECT_EQ(value, expected_bool[i].expected_value) << expected_bool[i].name;
  }

  const char* missing_bools[] = {
    installer_util::master_preferences::kDistroShowWelcomePage,
    installer_util::master_preferences::kDistroImportHistoryPref,
    installer_util::master_preferences::kDistroImportHomePagePref,
    installer_util::master_preferences::kDoNotRegisterForUpdateLaunch,
    installer_util::master_preferences::kMakeChromeDefault,
    installer_util::master_preferences::kMakeChromeDefaultForUser,
  };

  for (int i = 0; i < arraysize(missing_bools); ++i) {
    EXPECT_FALSE(prefs.GetBool(missing_bools[i], &value)) << missing_bools[i];
  }

  std::string str_value;
  EXPECT_FALSE(prefs.GetString(
      installer_util::master_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));

  int icon_index = 0;
  EXPECT_FALSE(prefs.GetInt(
      installer_util::master_preferences::kChromeShortcutIconIndex,
      &icon_index));
  EXPECT_EQ(icon_index, 0);

  int ping_delay = 90;
  EXPECT_FALSE(prefs.GetInt(
      installer_util::master_preferences::kDistroPingDelay, &ping_delay));
  EXPECT_EQ(ping_delay, 90);
}

TEST_F(MasterPreferencesTest, FirstRunTabs) {
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"something here\": true\n"
    "  },\n"
    "  \"first_run_tabs\": [\n"
    "     \"http://google.com/f1\",\n"
    "     \"https://google.com/f2\",\n"
    "     \"new_tab_page\"\n"
    "  ]\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs_file(), text, sizeof(text)));
  installer_util::MasterPreferences prefs(prefs_file());
  typedef std::vector<GURL> TabsVector;
  TabsVector tabs = prefs.GetFirstRunTabs();
  ASSERT_EQ(3, tabs.size());
  EXPECT_EQ(GURL("http://google.com/f1"), tabs[0]);
  EXPECT_EQ(GURL("https://google.com/f2"), tabs[1]);
  EXPECT_EQ(GURL("new_tab_page"), tabs[2]);
}

// In this test instead of using our synthetic json file, we use an
// actual test case from the extensions unittest. The hope here is that if
// they change something in the manifest this test will break, but in
// general it is expected the extension format to be backwards compatible.
TEST(MasterPrefsExtension, ValidateExtensionJSON) {
  FilePath prefs_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &prefs_path));
  prefs_path = prefs_path.AppendASCII("extensions")
      .AppendASCII("good").AppendASCII("Preferences");

  installer_util::MasterPreferences prefs(prefs_path);
  DictionaryValue* extensions = NULL;
  EXPECT_TRUE(prefs.GetExtensionsBlock(&extensions));
  int location = 0;
  EXPECT_TRUE(extensions->GetInteger(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.location", &location));
  int state = 0;
  EXPECT_TRUE(extensions->GetInteger(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.state", &state));
  std::string path;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.path", &path));
  std::string key;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.manifest.key", &key));
  std::string name;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.manifest.name", &name));
  std::string version;
  EXPECT_TRUE(extensions->GetString(
      "behllobkkfkfnphdnhnkndlbkcpglgmj.manifest.version", &version));
}

// Test that we are parsing master preferences correctly.
TEST_F(MasterPreferencesTest, GetInstallPreferencesTest) {
  // Create a temporary prefs file.
  FilePath prefs_file;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&prefs_file));
  const char text[] =
    "{ \n"
    "  \"distribution\": { \n"
    "     \"skip_first_run_ui\": true,\n"
    "     \"create_all_shortcuts\": false,\n"
    "     \"do_not_launch_chrome\": true,\n"
    "     \"system_level\": true,\n"
    "     \"verbose_logging\": false\n"
    "  }\n"
    "} \n";
  EXPECT_TRUE(file_util::WriteFile(prefs_file, text, sizeof(text)));

  // Make sure command line values override the values in master preferences.
  std::wstring cmd_str(
      L"setup.exe --installerdata=\"" + prefs_file.value() + L"\"");
  cmd_str.append(L" --create-all-shortcuts");
  cmd_str.append(L" --do-not-launch-chrome");
  cmd_str.append(L" --alt-desktop-shortcut");
  CommandLine cmd_line = CommandLine::FromString(cmd_str);
  installer_util::MasterPreferences prefs(cmd_line);

  // Check prefs that do not have any equivalent command line option.
  ExpectedBooleans expected_bool[] = {
    { installer_util::master_preferences::kDistroSkipFirstRunPref, true },
    { installer_util::master_preferences::kCreateAllShortcuts, true },
    { installer_util::master_preferences::kDoNotLaunchChrome, true },
    { installer_util::master_preferences::kAltShortcutText, true },
    { installer_util::master_preferences::kSystemLevel, true },
    { installer_util::master_preferences::kVerboseLogging, false },
  };

  // Now check that prefs got merged correctly.
  bool value = false;
  for (int i = 0; i < arraysize(expected_bool); ++i) {
    EXPECT_TRUE(prefs.GetBool(expected_bool[i].name, &value));
    EXPECT_EQ(value, expected_bool[i].expected_value) << expected_bool[i].name;
  }

  // Delete temporary prefs file.
  EXPECT_TRUE(file_util::Delete(prefs_file, false));

  // Check that if master prefs doesn't exist, we can still parse the common
  // prefs.
  cmd_str = L"setup.exe --create-all-shortcuts --do-not-launch-chrome"
            L" --alt-desktop-shortcut";
  cmd_line.ParseFromString(cmd_str);
  installer_util::MasterPreferences prefs2(cmd_line);
  ExpectedBooleans expected_bool2[] = {
    { installer_util::master_preferences::kCreateAllShortcuts, true },
    { installer_util::master_preferences::kDoNotLaunchChrome, true },
    { installer_util::master_preferences::kAltShortcutText, true },
  };

  for (int i = 0; i < arraysize(expected_bool2); ++i) {
    EXPECT_TRUE(prefs2.GetBool(expected_bool2[i].name, &value));
    EXPECT_EQ(value, expected_bool2[i].expected_value)
        << expected_bool2[i].name;
  }

  EXPECT_FALSE(prefs2.GetBool(
      installer_util::master_preferences::kSystemLevel, &value));
  EXPECT_FALSE(prefs2.GetBool(
      installer_util::master_preferences::kVerboseLogging, &value));
}

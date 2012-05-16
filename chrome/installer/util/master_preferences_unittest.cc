// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for master preferences related methods.

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
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
    "     \"chrome_shortcut_icon_index\": 1,\n"
    "     \"ping_delay\": 40\n"
    "  },\n"
    "  \"blah\": {\n"
    "     \"import_history\": false\n"
    "  }\n"
    "} \n";

  EXPECT_TRUE(file_util::WriteFile(prefs_file(), text, strlen(text)));
  installer::MasterPreferences prefs(prefs_file());

  const char* expected_true[] = {
    installer::master_preferences::kDistroSkipFirstRunPref,
    installer::master_preferences::kDistroShowWelcomePage,
    installer::master_preferences::kDistroImportSearchPref,
    installer::master_preferences::kDistroImportHistoryPref,
    installer::master_preferences::kDistroImportBookmarksPref,
    installer::master_preferences::kDistroImportHomePagePref,
    installer::master_preferences::kCreateAllShortcuts,
    installer::master_preferences::kDoNotLaunchChrome,
    installer::master_preferences::kMakeChromeDefault,
    installer::master_preferences::kMakeChromeDefaultForUser,
    installer::master_preferences::kSystemLevel,
    installer::master_preferences::kVerboseLogging,
    installer::master_preferences::kRequireEula,
    installer::master_preferences::kAltShortcutText,
  };

  for (int i = 0; i < arraysize(expected_true); ++i) {
    bool value = false;
    EXPECT_TRUE(prefs.GetBool(expected_true[i], &value));
    EXPECT_TRUE(value) << expected_true[i];
  }

  std::string str_value;
  EXPECT_TRUE(prefs.GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));
  EXPECT_STREQ("c:\\foo", str_value.c_str());

  int icon_index = 0;
  EXPECT_TRUE(prefs.GetInt(
      installer::master_preferences::kChromeShortcutIconIndex,
      &icon_index));
  EXPECT_EQ(icon_index, 1);
  int ping_delay = 90;
  EXPECT_TRUE(prefs.GetInt(installer::master_preferences::kDistroPingDelay,
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

  EXPECT_TRUE(file_util::WriteFile(prefs_file(), text, strlen(text)));
  installer::MasterPreferences prefs(prefs_file());

  ExpectedBooleans expected_bool[] = {
    { installer::master_preferences::kDistroSkipFirstRunPref, true },
    { installer::master_preferences::kDistroImportSearchPref, true },
    { installer::master_preferences::kDistroImportBookmarksPref, false },
    { installer::master_preferences::kCreateAllShortcuts, true },
    { installer::master_preferences::kDoNotLaunchChrome, true },
  };

  bool value = false;
  for (int i = 0; i < arraysize(expected_bool); ++i) {
    EXPECT_TRUE(prefs.GetBool(expected_bool[i].name, &value));
    EXPECT_EQ(value, expected_bool[i].expected_value) << expected_bool[i].name;
  }

  const char* missing_bools[] = {
    installer::master_preferences::kDistroShowWelcomePage,
    installer::master_preferences::kDistroImportHistoryPref,
    installer::master_preferences::kDistroImportHomePagePref,
    installer::master_preferences::kDoNotRegisterForUpdateLaunch,
    installer::master_preferences::kMakeChromeDefault,
    installer::master_preferences::kMakeChromeDefaultForUser,
  };

  for (int i = 0; i < arraysize(missing_bools); ++i) {
    EXPECT_FALSE(prefs.GetBool(missing_bools[i], &value)) << missing_bools[i];
  }

  std::string str_value;
  EXPECT_FALSE(prefs.GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &str_value));

  int icon_index = 0;
  EXPECT_FALSE(prefs.GetInt(
      installer::master_preferences::kChromeShortcutIconIndex,
      &icon_index));
  EXPECT_EQ(icon_index, 0);

  int ping_delay = 90;
  EXPECT_FALSE(prefs.GetInt(
      installer::master_preferences::kDistroPingDelay, &ping_delay));
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

  EXPECT_TRUE(file_util::WriteFile(prefs_file(), text, strlen(text)));
  installer::MasterPreferences prefs(prefs_file());
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

  installer::MasterPreferences prefs(prefs_path);
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
  EXPECT_TRUE(file_util::WriteFile(prefs_file, text, strlen(text)));

  // Make sure command line values override the values in master preferences.
  std::wstring cmd_str(
      L"setup.exe --installerdata=\"" + prefs_file.value() + L"\"");
  cmd_str.append(L" --create-all-shortcuts");
  cmd_str.append(L" --do-not-launch-chrome");
  cmd_str.append(L" --alt-desktop-shortcut");
  CommandLine cmd_line = CommandLine::FromString(cmd_str);
  installer::MasterPreferences prefs(cmd_line);

  // Check prefs that do not have any equivalent command line option.
  ExpectedBooleans expected_bool[] = {
    { installer::master_preferences::kDistroSkipFirstRunPref, true },
    { installer::master_preferences::kCreateAllShortcuts, true },
    { installer::master_preferences::kDoNotLaunchChrome, true },
    { installer::master_preferences::kAltShortcutText, true },
    { installer::master_preferences::kSystemLevel, true },
    { installer::master_preferences::kVerboseLogging, false },
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
  installer::MasterPreferences prefs2(cmd_line);
  ExpectedBooleans expected_bool2[] = {
    { installer::master_preferences::kCreateAllShortcuts, true },
    { installer::master_preferences::kDoNotLaunchChrome, true },
    { installer::master_preferences::kAltShortcutText, true },
  };

  for (int i = 0; i < arraysize(expected_bool2); ++i) {
    EXPECT_TRUE(prefs2.GetBool(expected_bool2[i].name, &value));
    EXPECT_EQ(value, expected_bool2[i].expected_value)
        << expected_bool2[i].name;
  }

  EXPECT_FALSE(prefs2.GetBool(
      installer::master_preferences::kSystemLevel, &value));
  EXPECT_FALSE(prefs2.GetBool(
      installer::master_preferences::kVerboseLogging, &value));
}

TEST_F(MasterPreferencesTest, TestDefaultInstallConfig) {
  std::wstringstream chrome_cmd, cf_cmd;
  chrome_cmd << "setup.exe";
  cf_cmd << "setup.exe --" << installer::switches::kChromeFrame;

  CommandLine chrome_install(CommandLine::FromString(chrome_cmd.str()));
  CommandLine cf_install(CommandLine::FromString(cf_cmd.str()));

  installer::MasterPreferences pref_chrome(chrome_install);
  installer::MasterPreferences pref_cf(cf_install);

  EXPECT_FALSE(pref_chrome.is_multi_install());
  EXPECT_TRUE(pref_chrome.install_chrome());
  EXPECT_FALSE(pref_chrome.install_chrome_frame());

  EXPECT_FALSE(pref_cf.is_multi_install());
  EXPECT_FALSE(pref_cf.install_chrome());
  EXPECT_TRUE(pref_cf.install_chrome_frame());
}

TEST_F(MasterPreferencesTest, TestMultiInstallConfig) {
  using installer::switches::kMultiInstall;
  using installer::switches::kChrome;
  using installer::switches::kChromeFrame;

  std::wstringstream chrome_cmd, cf_cmd, chrome_cf_cmd;
  chrome_cmd << "setup.exe --" << kMultiInstall << " --" << kChrome;
  cf_cmd << "setup.exe --" << kMultiInstall << " --" << kChromeFrame;
  chrome_cf_cmd << "setup.exe --" << kMultiInstall << " --" << kChrome <<
      " --" << kChromeFrame;

  CommandLine chrome_install(CommandLine::FromString(chrome_cmd.str()));
  CommandLine cf_install(CommandLine::FromString(cf_cmd.str()));
  CommandLine chrome_cf_install(CommandLine::FromString(chrome_cf_cmd.str()));

  installer::MasterPreferences pref_chrome(chrome_install);
  installer::MasterPreferences pref_cf(cf_install);
  installer::MasterPreferences pref_chrome_cf(chrome_cf_install);

  EXPECT_TRUE(pref_chrome.is_multi_install());
  EXPECT_TRUE(pref_chrome.install_chrome());
  EXPECT_FALSE(pref_chrome.install_chrome_frame());

  EXPECT_TRUE(pref_cf.is_multi_install());
  EXPECT_FALSE(pref_cf.install_chrome());
  EXPECT_TRUE(pref_cf.install_chrome_frame());

  EXPECT_TRUE(pref_chrome_cf.is_multi_install());
  EXPECT_TRUE(pref_chrome_cf.install_chrome());
  EXPECT_TRUE(pref_chrome_cf.install_chrome_frame());
}


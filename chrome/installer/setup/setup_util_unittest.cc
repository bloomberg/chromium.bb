// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>
#include <iostream>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class SetupUtilTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      test_dir_ = test_dir_.AppendASCII("SetupUtilTest");

      // Create a fresh, empty copy of this test directory.
      file_util::Delete(test_dir_, true);
      file_util::CreateDirectory(test_dir_);
      ASSERT_TRUE(file_util::PathExists(test_dir_));
    }

    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    FilePath test_dir_;
  };
};

// Test that we are parsing master preferences correctly.
TEST_F(SetupUtilTest, GetInstallPreferencesTest) {
  // Create a temporary prefs file.
  std::wstring prefs_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileName(&prefs_file));
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
  std::wstring cmd_str(L"setup.exe --installerdata=\"" + prefs_file + L"\"");
  cmd_str.append(L" --create-all-shortcuts");
  cmd_str.append(L" --do-not-launch-chrome");
  cmd_str.append(L" --alt-desktop-shortcut");
  CommandLine cmd_line(L"");
  cmd_line.ParseFromString(cmd_str);
  scoped_ptr<DictionaryValue> prefs(
      setup_util::GetInstallPreferences(cmd_line));
  EXPECT_TRUE(prefs.get() != NULL);

  // Check prefs that do not have any equivalent command line option.
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroSkipFirstRunPref));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDistroShowWelcomePage));

  // Now check that prefs got merged correctly.
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kCreateAllShortcuts));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDoNotLaunchChrome));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kAltShortcutText));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kSystemLevel));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kVerboseLogging));

  // Delete temporary prefs file.
  EXPECT_TRUE(file_util::Delete(prefs_file, false));

  // Check that if master prefs doesn't exist, we can still parse the common
  // prefs.
  cmd_str = L"setup.exe --create-all-shortcuts --do-not-launch-chrome"
            L" --alt-desktop-shortcut";
  cmd_line.ParseFromString(cmd_str);
  prefs.reset(setup_util::GetInstallPreferences(cmd_line));
  EXPECT_TRUE(prefs.get() != NULL);
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kCreateAllShortcuts));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kDoNotLaunchChrome));
  EXPECT_TRUE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kAltShortcutText));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kSystemLevel));
  EXPECT_FALSE(installer_util::GetDistroBooleanPreference(prefs.get(),
      installer_util::master_preferences::kVerboseLogging));
}

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTest, GetVersionFromDirTest) {
  // Create a version dir
  std::wstring chrome_dir(test_dir_.value());
  file_util::AppendToPath(&chrome_dir, L"1.0.0.0");
  CreateDirectory(chrome_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  scoped_ptr<installer::Version> version(
      setup_util::GetVersionFromDir(test_dir_.value()));
  ASSERT_TRUE(version->GetString() == L"1.0.0.0");

  file_util::Delete(chrome_dir, true);
  ASSERT_FALSE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(setup_util::GetVersionFromDir(test_dir_.value()) == NULL);

  chrome_dir = test_dir_.value();
  file_util::AppendToPath(&chrome_dir, L"ABC");
  CreateDirectory(chrome_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  ASSERT_TRUE(setup_util::GetVersionFromDir(test_dir_.value()) == NULL);

  chrome_dir = test_dir_.value();
  file_util::AppendToPath(&chrome_dir, L"2.3.4.5");
  CreateDirectory(chrome_dir.c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(chrome_dir));
  version.reset(setup_util::GetVersionFromDir(test_dir_.value()));
  ASSERT_TRUE(version->GetString() == L"2.3.4.5");
}

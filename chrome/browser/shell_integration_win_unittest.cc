// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShellIntegrationWinMigrateShortcutTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // A path to a random target.
    FilePath other_target;
    file_util::CreateTemporaryFileInDir(temp_dir_.path(), &other_target);

    // This doesn't need to actually have a base name of "chrome.exe".
    file_util::CreateTemporaryFileInDir(temp_dir_.path(), &chrome_exe_);

    chrome_app_id_ =
        ShellUtil::GetBrowserModelId(BrowserDistribution::GetDistribution(),
                                     true);

    // A temporary object to pass properties to AddTestShortcut().
    base::win::ShortcutProperties temp_properties;

    // Shortcut 0 doesn't point to chrome.exe and thus should never be migrated.
    temp_properties.set_target(other_target);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_dual_mode(false);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcut(temp_properties));

    // Shortcut 1 points to chrome.exe and thus should be migrated.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_dual_mode(false);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcut(temp_properties));

    // Shortcut 2 points to chrome.exe, but already has the right appid and thus
    // should only be migrated if dual_mode is desired.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_);
    temp_properties.set_dual_mode(false);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcut(temp_properties));

    // Shortcut 3 is like shortcut 2 except it has dual_mode and thus should
    // never be migrated.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_);
    temp_properties.set_dual_mode(true);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcut(temp_properties));

    // Shortcut 4 is like shortcut 1, but it's appid is a prefix of the expected
    // appid instead of being totally different.
    string16 chrome_app_id_is_prefix(chrome_app_id_);
    chrome_app_id_is_prefix.push_back(L'1');
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_is_prefix);
    temp_properties.set_dual_mode(false);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcut(temp_properties));

    // Shortcut 5 is like shortcut 1, but it's appid is of the same size as the
    // expected appid.
    string16 same_size_as_chrome_app_id(L'1', chrome_app_id_.size());
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(same_size_as_chrome_app_id);
    temp_properties.set_dual_mode(false);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcut(temp_properties));

    // Shortcut 6 doesn't have an app_id, nor is dual_mode even set; they should
    // be set as expected upon migration.
    base::win::ShortcutProperties no_properties;
    no_properties.set_target(chrome_exe_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcut(no_properties));
  }

  void AddTestShortcut(
      const base::win::ShortcutProperties& shortcut_properties) {
    shortcuts_properties_.push_back(shortcut_properties);
    FilePath shortcut_path =
        temp_dir_.path().Append(L"Shortcut " +
                                base::IntToString16(shortcuts_.size()) +
                                installer::kLnkExt);
    shortcuts_.push_back(shortcut_path);
    ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
        shortcut_path, shortcut_properties,
        base::win::SHORTCUT_CREATE_ALWAYS));
  }

  base::win::ScopedCOMInitializer com_initializer_;
  base::ScopedTempDir temp_dir_;

  // The path to a fake chrome.exe.
  FilePath chrome_exe_;

  // Test shortcuts.
  std::vector<FilePath> shortcuts_;

  // Initial properties for the test shortcuts.
  std::vector<base::win::ShortcutProperties> shortcuts_properties_;

  // Chrome's AppUserModelId.
  string16 chrome_app_id_;
};

}  // namespace

// Test migration when not checking for dual mode.
TEST_F(ShellIntegrationWinMigrateShortcutTest, DontCheckDualMode) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  EXPECT_EQ(4,
            ShellIntegration::MigrateShortcutsInPathInternal(
                chrome_exe_, temp_dir_.path(), false));

  // Only shortcut 1, 4, 5, and 6 should have been migrated.
  shortcuts_properties_[1].set_app_id(chrome_app_id_);
  shortcuts_properties_[4].set_app_id(chrome_app_id_);
  shortcuts_properties_[5].set_app_id(chrome_app_id_);
  shortcuts_properties_[6].set_app_id(chrome_app_id_);

  for (size_t i = 0; i < shortcuts_.size(); ++i)
    base::win::ValidateShortcut(shortcuts_[i], shortcuts_properties_[i]);
}

// Test migration when also checking for dual mode.
TEST_F(ShellIntegrationWinMigrateShortcutTest, CheckDualMode) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  EXPECT_EQ(5,
            ShellIntegration::MigrateShortcutsInPathInternal(
                chrome_exe_, temp_dir_.path(), true));

  // Shortcut 1, 4, 5, and 6 should have had both their app_id and dual_mode
  // properties fixed and shortcut 2 should also have had it's dual_mode
  // property fixed.
  shortcuts_properties_[1].set_app_id(chrome_app_id_);
  shortcuts_properties_[4].set_app_id(chrome_app_id_);
  shortcuts_properties_[5].set_app_id(chrome_app_id_);
  shortcuts_properties_[6].set_app_id(chrome_app_id_);

  shortcuts_properties_[1].set_dual_mode(true);
  shortcuts_properties_[2].set_dual_mode(true);
  shortcuts_properties_[4].set_dual_mode(true);
  shortcuts_properties_[5].set_dual_mode(true);
  shortcuts_properties_[6].set_dual_mode(true);

  for (size_t i = 0; i < shortcuts_.size(); ++i)
    base::win::ValidateShortcut(shortcuts_[i], shortcuts_properties_[i]);
}

TEST(ShellIntegrationWinTest, GetAppModelIdForProfileTest) {
  const string16 base_app_id(
      BrowserDistribution::GetDistribution()->GetBaseAppId());

  // Empty profile path should get chrome::kBrowserAppID
  FilePath empty_path;
  EXPECT_EQ(base_app_id,
            ShellIntegration::GetAppModelIdForProfile(base_app_id, empty_path));

  // Default profile path should get chrome::kBrowserAppID
  FilePath default_user_data_dir;
  chrome::GetDefaultUserDataDirectory(&default_user_data_dir);
  FilePath default_profile_path =
      default_user_data_dir.AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(base_app_id,
            ShellIntegration::GetAppModelIdForProfile(base_app_id,
                                                      default_profile_path));

  // Non-default profile path should get chrome::kBrowserAppID joined with
  // profile info.
  FilePath profile_path(FILE_PATH_LITERAL("root"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("udd"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("User Data - Test"));
  EXPECT_EQ(base_app_id + L".udd.UserDataTest",
            ShellIntegration::GetAppModelIdForProfile(base_app_id,
                                                      profile_path));
}

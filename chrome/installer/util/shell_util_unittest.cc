// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/shell_util.h"

#include <vector>

#include "base/file_util.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShellUtilShortcutTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    dist_ = BrowserDistribution::GetDistribution();
    ASSERT_TRUE(dist_ != NULL);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.path().Append(installer::kChromeExe);
    EXPECT_EQ(0, file_util::WriteFile(chrome_exe_, "", 0));

    ASSERT_TRUE(fake_user_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_default_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_start_menu_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_start_menu_.CreateUniqueTempDir());
    ASSERT_TRUE(PathService::Override(base::DIR_USER_DESKTOP,
                                      fake_user_desktop_.path()));
    ASSERT_TRUE(PathService::Override(base::DIR_COMMON_DESKTOP,
                                      fake_common_desktop_.path()));
    ASSERT_TRUE(PathService::Override(base::DIR_USER_QUICK_LAUNCH,
                                      fake_user_quick_launch_.path()));
    ASSERT_TRUE(PathService::Override(base::DIR_DEFAULT_USER_QUICK_LAUNCH,
                                      fake_default_user_quick_launch_.path()));
    ASSERT_TRUE(PathService::Override(base::DIR_START_MENU,
                                      fake_start_menu_.path()));
    ASSERT_TRUE(PathService::Override(base::DIR_COMMON_START_MENU,
                                      fake_common_start_menu_.path()));

    FilePath icon_path;
    file_util::CreateTemporaryFileInDir(temp_dir_.path(), &icon_path);
    test_properties_.reset(
        new ShellUtil::ChromeShortcutProperties(ShellUtil::CURRENT_USER));
    test_properties_->set_chrome_exe(chrome_exe_);
    test_properties_->set_arguments(L"--test --chrome");
    test_properties_->set_description(L"Makes polar bears dance.");
    test_properties_->set_icon(icon_path);
    test_properties_->set_app_id(L"Polar.Bear");
    test_properties_->set_dual_mode(true);
  }

  // Validates that the shortcut at |location| matches |properties| (and
  // implicit default properties) for |dist|.
  // Note: This method doesn't verify the |pin_to_taskbar| property as it
  // implies real (non-mocked) state which is flaky to test.
  void ValidateChromeShortcut(
      ShellUtil::ChromeShortcutLocation location,
      BrowserDistribution* dist,
      const ShellUtil::ChromeShortcutProperties& properties) {
    FilePath expected_path;
    switch (location) {
      case ShellUtil::SHORTCUT_DESKTOP:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_user_desktop_.path() : fake_common_desktop_.path();
        break;
      case ShellUtil::SHORTCUT_QUICK_LAUNCH:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_user_quick_launch_.path() :
            fake_default_user_quick_launch_.path();
        break;
      case ShellUtil::SHORTCUT_START_MENU:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_start_menu_.path() : fake_common_start_menu_.path();
        expected_path = expected_path.Append(dist_->GetAppShortCutName());
        break;
      default:
        ADD_FAILURE() << "Unknown location";
        return;
    }

    string16 shortcut_name;
    if (properties.has_shortcut_name()) {
      shortcut_name = properties.shortcut_name;
    } else {
      shortcut_name = dist_->GetAppShortCutName();
    }
    shortcut_name.append(installer::kLnkExt);
    expected_path = expected_path.Append(shortcut_name);

    base::win::ShortcutProperties expected_properties;
    expected_properties.set_target(chrome_exe_);
    expected_properties.set_working_dir(chrome_exe_.DirName());

    if (properties.has_arguments())
      expected_properties.set_arguments(properties.arguments);
    else
      expected_properties.set_arguments(string16());

    if (properties.has_description())
      expected_properties.set_description(properties.description);
    else
      expected_properties.set_description(dist->GetAppDescription());

    if (properties.has_icon()) {
      expected_properties.set_icon(properties.icon, 0);
    } else {
      int icon_index = dist->GetIconIndex();
      expected_properties.set_icon(chrome_exe_, icon_index);
    }

    if (properties.has_app_id()) {
      expected_properties.set_app_id(properties.app_id);
    } else {
      // Tests are always seen as user-level installs in ShellUtil.
      expected_properties.set_app_id(ShellUtil::GetBrowserModelId(dist, true));
    }

    if (properties.has_dual_mode())
      expected_properties.set_dual_mode(properties.dual_mode);
    else
      expected_properties.set_dual_mode(false);

    base::win::ValidateShortcut(expected_path, expected_properties);
  }

  BrowserDistribution* dist_;

  // A ChromeShortcutProperties object with common properties set already.
  scoped_ptr<ShellUtil::ChromeShortcutProperties> test_properties_;

  ScopedTempDir temp_dir_;
  ScopedTempDir fake_user_desktop_;
  ScopedTempDir fake_common_desktop_;
  ScopedTempDir fake_user_quick_launch_;
  ScopedTempDir fake_default_user_quick_launch_;
  ScopedTempDir fake_start_menu_;
  ScopedTempDir fake_common_start_menu_;

  FilePath chrome_exe_;
};

}  // namespace

TEST_F(ShellUtilShortcutTest, GetShortcutPath) {
  FilePath path;
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_DESKTOP, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_user_desktop_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_DESKTOP, dist_,
                             ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_common_desktop_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_user_quick_launch_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_,
                             ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_default_user_quick_launch_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_START_MENU, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_start_menu_.path().Append(dist_->GetAppShortCutName()), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_START_MENU, dist_,
                             ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_common_start_menu_.path().Append(dist_->GetAppShortCutName()),
            path);
}

TEST_F(ShellUtilShortcutTest, CreateChromeExeShortcutWithDefaultProperties) {
  ShellUtil::ChromeShortcutProperties properties(ShellUtil::CURRENT_USER);
  properties.set_chrome_exe(chrome_exe_);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, properties,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_DESKTOP, dist_, properties);
}

TEST_F(ShellUtilShortcutTest, CreateStartMenuShortcutWithAllProperties) {
  test_properties_->set_shortcut_name(L"Bobo le shortcut");
  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_START_MENU, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_START_MENU, dist_,
                         *test_properties_);
}

TEST_F(ShellUtilShortcutTest, ReplaceSystemLevelQuickLaunchShortcut) {
  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));

  ShellUtil::ChromeShortcutProperties new_properties(ShellUtil::SYSTEM_LEVEL);
  new_properties.set_chrome_exe(chrome_exe_);
  new_properties.set_description(L"New description");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_, new_properties,
                  ShellUtil::SHORTCUT_REPLACE_EXISTING));

  // Expect the properties set in |new_properties| to be set as above and
  // properties that don't have a default value to be set back to their default
  // (as validated in ValidateChromeShortcut()) or unset if they don't .
  ShellUtil::ChromeShortcutProperties expected_properties(new_properties);
  expected_properties.set_arguments(string16());
  expected_properties.set_dual_mode(false);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, UpdateQuickLaunchShortcutArguments) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));

  ShellUtil::ChromeShortcutProperties updated_properties(
      ShellUtil::CURRENT_USER);
  updated_properties.set_arguments(L"--updated --arguments");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_, updated_properties,
                  ShellUtil::SHORTCUT_UPDATE_EXISTING));

  // Expect the properties set in |updated_properties| to be set as above and
  // all other properties to remain unchanged.
  ShellUtil::ChromeShortcutProperties expected_properties(*test_properties_);
  expected_properties.set_arguments(updated_properties.arguments);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_QUICK_LAUNCH, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevel) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevelWithSystemLevelPresent) {
  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);

  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::PathExists(
      fake_common_desktop_.path().Append(shortcut_name)));

  test_properties_->level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ASSERT_FALSE(file_util::PathExists(
      fake_user_desktop_.path().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, CreateAlwaysUserWithSystemLevelPresent) {
  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);

  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::PathExists(
      fake_common_desktop_.path().Append(shortcut_name)));

  test_properties_->level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::PathExists(
      fake_user_desktop_.path().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, RemoveChromeShortcut) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));

  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);
  FilePath shortcut_path(fake_user_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveChromeShortcut(
      ShellUtil::SHORTCUT_DESKTOP, dist_, ShellUtil::CURRENT_USER, NULL));
  ASSERT_FALSE(file_util::PathExists(shortcut_path));
  ASSERT_TRUE(file_util::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveSystemLevelChromeShortcut) {
  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));

  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);
  FilePath shortcut_path(fake_common_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveChromeShortcut(
      ShellUtil::SHORTCUT_DESKTOP, dist_, ShellUtil::SYSTEM_LEVEL, NULL));
  ASSERT_FALSE(file_util::PathExists(shortcut_path));
  ASSERT_TRUE(file_util::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveChromeShortcutWithSpecialName) {
  static const wchar_t kSpecialName[] = L"I'm special";
  test_properties_->set_shortcut_name(kSpecialName);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_DESKTOP, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));

  string16 shortcut_name(string16(kSpecialName).append(installer::kLnkExt));
  FilePath shortcut_path(fake_user_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveChromeShortcut(
      ShellUtil::SHORTCUT_DESKTOP, dist_, ShellUtil::CURRENT_USER,
      &string16(kSpecialName)));
  ASSERT_FALSE(file_util::PathExists(shortcut_path));
  ASSERT_TRUE(file_util::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, CreateMultipleStartMenuShortcutsAndRemoveFolder) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_START_MENU, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));
  test_properties_->set_shortcut_name(L"A second shortcut");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateChromeShortcut(
                  ShellUtil::SHORTCUT_START_MENU, dist_, *test_properties_,
                  ShellUtil::SHORTCUT_CREATE_ALWAYS));

  FilePath shortcut_folder(
      fake_start_menu_.path().Append(dist_->GetAppShortCutName()));
  file_util::FileEnumerator file_counter (shortcut_folder, false,
                                          file_util::FileEnumerator::FILES);
  int count = 0;
  while (!file_counter.Next().empty())
    ++count;
  EXPECT_EQ(2, count);

  ASSERT_TRUE(file_util::PathExists(shortcut_folder));
  ASSERT_TRUE(ShellUtil::RemoveChromeShortcut(
      ShellUtil::SHORTCUT_START_MENU, dist_, ShellUtil::CURRENT_USER, NULL));
  ASSERT_FALSE(file_util::PathExists(shortcut_folder));
}

TEST(ShellUtilTest, BuildAppModelIdBasic) {
  std::vector<string16> components;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  const string16 base_app_id(dist->GetBaseAppId());
  components.push_back(base_app_id);
  ASSERT_EQ(base_app_id, ShellUtil::BuildAppModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdManySmall) {
  std::vector<string16> components;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  const string16 suffixed_app_id(dist->GetBaseAppId().append(L".gab"));
  components.push_back(suffixed_app_id);
  components.push_back(L"Default");
  components.push_back(L"Test");
  ASSERT_EQ(suffixed_app_id + L".Default.Test",
            ShellUtil::BuildAppModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdLongUsernameNormalProfile) {
  std::vector<string16> components;
  const string16 long_appname(
      L"Chrome.a_user_who_has_a_crazy_long_name_with_some_weird@symbols_in_it_"
      L"that_goes_over_64_characters");
  components.push_back(long_appname);
  components.push_back(L"Default");
  ASSERT_EQ(L"Chrome.a_user_wer_64_characters.Default",
            ShellUtil::BuildAppModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdLongEverything) {
  std::vector<string16> components;
  const string16 long_appname(
      L"Chrome.a_user_who_has_a_crazy_long_name_with_some_weird@symbols_in_it_"
      L"that_goes_over_64_characters");
  components.push_back(long_appname);
  components.push_back(
      L"A_crazy_profile_name_not_even_sure_whether_that_is_possible");
  const string16 constructed_app_id(ShellUtil::BuildAppModelId(components));
  ASSERT_LE(constructed_app_id.length(), installer::kMaxAppModelIdLength);
  ASSERT_EQ(L"Chrome.a_user_wer_64_characters.A_crazy_profilethat_is_possible",
            constructed_app_id);
}

TEST(ShellUtilTest, GetUserSpecificRegistrySuffix) {
  string16 suffix;
  ASSERT_TRUE(ShellUtil::GetUserSpecificRegistrySuffix(&suffix));
  ASSERT_TRUE(StartsWith(suffix, L".", true));
  ASSERT_EQ(27, suffix.length());
  ASSERT_TRUE(ContainsOnlyChars(suffix.substr(1),
                                L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"));
}

TEST(ShellUtilTest, GetOldUserSpecificRegistrySuffix) {
  string16 suffix;
  ASSERT_TRUE(ShellUtil::GetOldUserSpecificRegistrySuffix(&suffix));
  ASSERT_TRUE(StartsWith(suffix, L".", true));

  wchar_t user_name[256];
  DWORD size = arraysize(user_name);
  ASSERT_NE(0, ::GetUserName(user_name, &size));
  ASSERT_GE(size, 1U);
  ASSERT_STREQ(user_name, suffix.substr(1).c_str());
}

TEST(ShellUtilTest, ByteArrayToBase32) {
  // Tests from http://tools.ietf.org/html/rfc4648#section-10.
  const unsigned char test_array[] = { 'f', 'o', 'o', 'b', 'a', 'r' };

  const string16 expected[] = { L"", L"MY", L"MZXQ", L"MZXW6", L"MZXW6YQ",
                                L"MZXW6YTB", L"MZXW6YTBOI"};

  // Run the tests, with one more letter in the input every pass.
  for (int i = 0; i < arraysize(expected); ++i) {
    ASSERT_EQ(expected[i],
              ShellUtil::ByteArrayToBase32(test_array, i));
  }
}

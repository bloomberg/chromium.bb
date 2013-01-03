// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/shell_util.h"

#include <vector>

#include "base/base_paths.h"
#include "base/base_paths_win.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(huangs): Separate this into generic shortcut tests and Chrome-specific
// tests. Specifically, we should not overly rely on getting shortcut properties
// from product_->AddDefaultShortcutProperties().
class ShellUtilShortcutTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    dist_ = BrowserDistribution::GetDistribution();
    ASSERT_TRUE(dist_ != NULL);
    product_.reset(new installer::Product(dist_));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.path().Append(installer::kChromeExe);
    EXPECT_EQ(0, file_util::WriteFile(chrome_exe_, "", 0));

    ASSERT_TRUE(fake_user_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_default_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_start_menu_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_start_menu_.CreateUniqueTempDir());
    user_desktop_override_.reset(
        new base::ScopedPathOverride(base::DIR_USER_DESKTOP,
                                     fake_user_desktop_.path()));
    common_desktop_override_.reset(
        new base::ScopedPathOverride(base::DIR_COMMON_DESKTOP,
                                     fake_common_desktop_.path()));
    user_quick_launch_override_.reset(
        new base::ScopedPathOverride(base::DIR_USER_QUICK_LAUNCH,
                                     fake_user_quick_launch_.path()));
    default_user_quick_launch_override_.reset(
        new base::ScopedPathOverride(base::DIR_DEFAULT_USER_QUICK_LAUNCH,
                                     fake_default_user_quick_launch_.path()));
    start_menu_override_.reset(
        new base::ScopedPathOverride(base::DIR_START_MENU,
                                     fake_start_menu_.path()));
    common_start_menu_override_.reset(
        new base::ScopedPathOverride(base::DIR_COMMON_START_MENU,
                                     fake_common_start_menu_.path()));


    FilePath icon_path;
    file_util::CreateTemporaryFileInDir(temp_dir_.path(), &icon_path);
    test_properties_.reset(
        new ShellUtil::ShortcutProperties(ShellUtil::CURRENT_USER));
    test_properties_->set_target(chrome_exe_);
    test_properties_->set_arguments(L"--test --chrome");
    test_properties_->set_description(L"Makes polar bears dance.");
    test_properties_->set_icon(icon_path, 0);
    test_properties_->set_app_id(L"Polar.Bear");
    test_properties_->set_dual_mode(true);
  }

  // Validates that the shortcut at |location| matches |properties| (and
  // implicit default properties) for |dist|.
  // Note: This method doesn't verify the |pin_to_taskbar| property as it
  // implies real (non-mocked) state which is flaky to test.
  void ValidateChromeShortcut(
      ShellUtil::ShortcutLocation location,
      BrowserDistribution* dist,
      const ShellUtil::ShortcutProperties& properties) {
    FilePath expected_path;
    switch (location) {
      case ShellUtil::SHORTCUT_LOCATION_DESKTOP:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_user_desktop_.path() : fake_common_desktop_.path();
        break;
      case ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_user_quick_launch_.path() :
            fake_default_user_quick_launch_.path();
        break;
      case ShellUtil::SHORTCUT_LOCATION_START_MENU:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_start_menu_.path() : fake_common_start_menu_.path();
        expected_path = expected_path.Append(dist_->GetAppShortCutName());
        break;
      default:
        ADD_FAILURE() << "Unknown location";
        return;
    }

    string16 shortcut_name;
    if (properties.has_shortcut_name())
      shortcut_name = properties.shortcut_name;
    else
      shortcut_name = dist_->GetAppShortCutName();
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
  scoped_ptr<installer::Product> product_;

  // A ShellUtil::ShortcutProperties object with common properties set already.
  scoped_ptr<ShellUtil::ShortcutProperties> test_properties_;

  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir fake_user_desktop_;
  base::ScopedTempDir fake_common_desktop_;
  base::ScopedTempDir fake_user_quick_launch_;
  base::ScopedTempDir fake_default_user_quick_launch_;
  base::ScopedTempDir fake_start_menu_;
  base::ScopedTempDir fake_common_start_menu_;
  scoped_ptr<base::ScopedPathOverride> user_desktop_override_;
  scoped_ptr<base::ScopedPathOverride> common_desktop_override_;
  scoped_ptr<base::ScopedPathOverride> user_quick_launch_override_;
  scoped_ptr<base::ScopedPathOverride> default_user_quick_launch_override_;
  scoped_ptr<base::ScopedPathOverride> start_menu_override_;
  scoped_ptr<base::ScopedPathOverride> common_start_menu_override_;

  FilePath chrome_exe_;
};

}  // namespace

TEST_F(ShellUtilShortcutTest, GetShortcutPath) {
  FilePath path;
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_user_desktop_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                             ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_common_desktop_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_user_quick_launch_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, dist_,
                             ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_default_user_quick_launch_.path(), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_start_menu_.path().Append(dist_->GetAppShortCutName()), path);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_,
                             ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_common_start_menu_.path().Append(dist_->GetAppShortCutName()),
            path);
}

TEST_F(ShellUtilShortcutTest, CreateChromeExeShortcutWithDefaultProperties) {
  ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
  product_->AddDefaultShortcutProperties(chrome_exe_, &properties);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, properties,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         properties);
}

TEST_F(ShellUtilShortcutTest, CreateStartMenuShortcutWithAllProperties) {
  test_properties_->set_shortcut_name(L"Bobo le shortcut");
  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU,
                  dist_, *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_,
                         *test_properties_);
}

TEST_F(ShellUtilShortcutTest, ReplaceSystemLevelQuickLaunchShortcut) {
  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                  dist_, *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  ShellUtil::ShortcutProperties new_properties(ShellUtil::SYSTEM_LEVEL);
  product_->AddDefaultShortcutProperties(chrome_exe_, &new_properties);
  new_properties.set_description(L"New description");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                  dist_, new_properties,
                  ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING));

  // Expect the properties set in |new_properties| to be set as above and
  // properties that don't have a default value to be set back to their default
  // (as validated in ValidateChromeShortcut()) or unset if they don't .
  ShellUtil::ShortcutProperties expected_properties(new_properties);
  expected_properties.set_arguments(string16());
  expected_properties.set_dual_mode(false);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, UpdateQuickLaunchShortcutArguments) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                  dist_, *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  // Only changing one property, don't need all the defaults.
  ShellUtil::ShortcutProperties updated_properties(ShellUtil::CURRENT_USER);
  updated_properties.set_arguments(L"--updated --arguments");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                  dist_, updated_properties,
                  ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING));

  // Expect the properties set in |updated_properties| to be set as above and
  // all other properties to remain unchanged.
  ShellUtil::ShortcutProperties expected_properties(*test_properties_);
  expected_properties.set_arguments(updated_properties.arguments);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, UpdateAddDualModeToStartMenuShortcut) {
  ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
  product_->AddDefaultShortcutProperties(chrome_exe_, &properties);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_, properties,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  ShellUtil::ShortcutProperties added_properties(ShellUtil::CURRENT_USER);
  added_properties.set_dual_mode(true);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_,
                  added_properties, ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING));

  ShellUtil::ShortcutProperties expected_properties(properties);
  expected_properties.set_dual_mode(true);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevel) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         *test_properties_);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevelWithSystemLevelPresent) {
  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);

  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::PathExists(
      fake_common_desktop_.path().Append(shortcut_name)));

  test_properties_->level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ASSERT_FALSE(file_util::PathExists(
      fake_user_desktop_.path().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevelStartMenu) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU,
                  dist_, *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_,
                         *test_properties_);
}

TEST_F(ShellUtilShortcutTest, CreateAlwaysUserWithSystemLevelPresent) {
  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);

  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::PathExists(
      fake_common_desktop_.path().Append(shortcut_name)));

  test_properties_->level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::PathExists(
      fake_user_desktop_.path().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, RemoveChromeShortcut) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);
  FilePath shortcut_path(fake_user_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, chrome_exe_,
      ShellUtil::CURRENT_USER, NULL));
  ASSERT_FALSE(file_util::PathExists(shortcut_path));
  ASSERT_TRUE(file_util::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveSystemLevelChromeShortcut) {
  test_properties_->level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);
  FilePath shortcut_path(fake_common_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, chrome_exe_,
      ShellUtil::SYSTEM_LEVEL, NULL));
  ASSERT_FALSE(file_util::PathExists(shortcut_path));
  ASSERT_TRUE(file_util::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveChromeShortcutWithSpecialName) {
  static const wchar_t kSpecialName[] = L"I'm special";
  test_properties_->set_shortcut_name(kSpecialName);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  string16 shortcut_name(string16(kSpecialName).append(installer::kLnkExt));
  FilePath shortcut_path(fake_user_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, chrome_exe_,
      ShellUtil::CURRENT_USER, &string16(kSpecialName)));
  ASSERT_FALSE(file_util::PathExists(shortcut_path));
  ASSERT_TRUE(file_util::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveMultipleChromeShortcuts) {
  const wchar_t kShortcutName1[] = L"Chrome 1";
  const wchar_t kShortcutName2[] = L"Chrome 2";

  test_properties_->set_shortcut_name(kShortcutName1);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  string16 shortcut1_name(string16(kShortcutName1).append(installer::kLnkExt));
  FilePath shortcut1_path(fake_user_desktop_.path().Append(shortcut1_name));
  ASSERT_TRUE(file_util::PathExists(shortcut1_path));

  test_properties_->set_shortcut_name(kShortcutName2);
  test_properties_->set_arguments(L"--profile-directory=\"Profile 2\"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  string16 shortcut2_name(string16(kShortcutName2).append(installer::kLnkExt));
  FilePath shortcut2_path(fake_user_desktop_.path().Append(shortcut2_name));
  ASSERT_TRUE(file_util::PathExists(shortcut2_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, chrome_exe_,
      ShellUtil::CURRENT_USER, NULL));
  ASSERT_FALSE(file_util::PathExists(shortcut1_path));
  ASSERT_FALSE(file_util::PathExists(shortcut2_path));
  ASSERT_TRUE(file_util::PathExists(shortcut1_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, CreateMultipleStartMenuShortcutsAndRemoveFolder) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU,
                  dist_, *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  test_properties_->set_shortcut_name(L"A second shortcut");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU,
                  dist_, *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  FilePath shortcut_folder(
      fake_start_menu_.path().Append(dist_->GetAppShortCutName()));
  file_util::FileEnumerator file_counter(shortcut_folder, false,
                                         file_util::FileEnumerator::FILES);
  int count = 0;
  while (!file_counter.Next().empty())
    ++count;
  EXPECT_EQ(2, count);

  ASSERT_TRUE(file_util::PathExists(shortcut_folder));
  ASSERT_TRUE(ShellUtil::RemoveShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU, dist_, chrome_exe_,
      ShellUtil::CURRENT_USER, NULL));
  ASSERT_FALSE(file_util::PathExists(shortcut_folder));
}

TEST_F(ShellUtilShortcutTest, DontRemoveChromeShortcutIfPointsToAnotherChrome) {
  base::ScopedTempDir other_exe_dir;
  ASSERT_TRUE(other_exe_dir.CreateUniqueTempDir());
  FilePath other_chrome_exe =
      other_exe_dir.path().Append(installer::kChromeExe);
  EXPECT_EQ(0, file_util::WriteFile(other_chrome_exe, "", 0));

  test_properties_->set_target(other_chrome_exe);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                  *test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  string16 shortcut_name(dist_->GetAppShortCutName() + installer::kLnkExt);
  FilePath shortcut_path(fake_user_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));

  // The shortcut shouldn't be removed as it was installed pointing to
  // |other_chrome_exe| and RemoveChromeShortcut() is being told that the
  // removed shortcut should point to |chrome_exe_|.
  ASSERT_TRUE(ShellUtil::RemoveShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, chrome_exe_,
      ShellUtil::CURRENT_USER, NULL));
  ASSERT_TRUE(file_util::PathExists(shortcut_path));
  ASSERT_TRUE(file_util::PathExists(shortcut_path.DirName()));
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

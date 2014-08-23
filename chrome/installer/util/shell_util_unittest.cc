// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/shell_util.h"

#include <vector>

#include "base/base_paths.h"
#include "base/base_paths_win.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kManganeseExe[] = L"manganese.exe";
const wchar_t kIronExe[] = L"iron.exe";
const wchar_t kOtherIco[] = L"other.ico";

// TODO(huangs): Separate this into generic shortcut tests and Chrome-specific
// tests. Specifically, we should not overly rely on getting shortcut properties
// from product_->AddDefaultShortcutProperties().
class ShellUtilShortcutTest : public testing::Test {
 protected:
  ShellUtilShortcutTest() : test_properties_(ShellUtil::CURRENT_USER) {}

  virtual void SetUp() OVERRIDE {
    dist_ = BrowserDistribution::GetDistribution();
    ASSERT_TRUE(dist_ != NULL);
    product_.reset(new installer::Product(dist_));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.path().Append(installer::kChromeExe);
    EXPECT_EQ(0, base::WriteFile(chrome_exe_, "", 0));

    manganese_exe_ = temp_dir_.path().Append(kManganeseExe);
    EXPECT_EQ(0, base::WriteFile(manganese_exe_, "", 0));

    iron_exe_ = temp_dir_.path().Append(kIronExe);
    EXPECT_EQ(0, base::WriteFile(iron_exe_, "", 0));

    other_ico_ = temp_dir_.path().Append(kOtherIco);
    EXPECT_EQ(0, base::WriteFile(other_ico_, "", 0));

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
    start_menu_override_.reset(
        new base::ScopedPathOverride(base::DIR_START_MENU,
                                     fake_start_menu_.path()));
    common_start_menu_override_.reset(
        new base::ScopedPathOverride(base::DIR_COMMON_START_MENU,
                                     fake_common_start_menu_.path()));

    base::FilePath icon_path;
    base::CreateTemporaryFileInDir(temp_dir_.path(), &icon_path);
    test_properties_.set_target(chrome_exe_);
    test_properties_.set_arguments(L"--test --chrome");
    test_properties_.set_description(L"Makes polar bears dance.");
    test_properties_.set_icon(icon_path, 7);
    test_properties_.set_app_id(L"Polar.Bear");
    test_properties_.set_dual_mode(true);
  }

  // Returns the expected path of a test shortcut. Returns an empty FilePath on
  // failure.
  base::FilePath GetExpectedShortcutPath(
      ShellUtil::ShortcutLocation location,
      BrowserDistribution* dist,
      const ShellUtil::ShortcutProperties& properties) {
    base::FilePath expected_path;
    switch (location) {
      case ShellUtil::SHORTCUT_LOCATION_DESKTOP:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_user_desktop_.path() : fake_common_desktop_.path();
        break;
      case ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH:
        expected_path = fake_user_quick_launch_.path();
        break;
      case ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR:
        expected_path = (properties.level == ShellUtil::CURRENT_USER) ?
            fake_start_menu_.path() : fake_common_start_menu_.path();
        expected_path = expected_path.Append(
            dist_->GetStartMenuShortcutSubfolder(
                BrowserDistribution::SUBFOLDER_CHROME));
        break;
      default:
        ADD_FAILURE() << "Unknown location";
        return base::FilePath();
    }

    base::string16 shortcut_name = properties.has_shortcut_name() ?
        properties.shortcut_name :
        dist_->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME);
    shortcut_name.append(installer::kLnkExt);
    return expected_path.Append(shortcut_name);
  }

  // Validates that the shortcut at |location| matches |properties| (and
  // implicit default properties) for |dist|.
  // Note: This method doesn't verify the |pin_to_taskbar| property as it
  // implies real (non-mocked) state which is flaky to test.
  void ValidateChromeShortcut(
      ShellUtil::ShortcutLocation location,
      BrowserDistribution* dist,
      const ShellUtil::ShortcutProperties& properties) {
    base::FilePath expected_path(
        GetExpectedShortcutPath(location, dist, properties));

    base::win::ShortcutProperties expected_properties;
    if (properties.has_target()) {
      expected_properties.set_target(properties.target);
      expected_properties.set_working_dir(properties.target.DirName());
    } else {
      expected_properties.set_target(chrome_exe_);
      expected_properties.set_working_dir(chrome_exe_.DirName());
    }

    if (properties.has_arguments())
      expected_properties.set_arguments(properties.arguments);
    else
      expected_properties.set_arguments(base::string16());

    if (properties.has_description())
      expected_properties.set_description(properties.description);
    else
      expected_properties.set_description(dist->GetAppDescription());

    if (properties.has_icon()) {
      expected_properties.set_icon(properties.icon, properties.icon_index);
    } else {
      int icon_index = dist->GetIconIndex(BrowserDistribution::SHORTCUT_CHROME);
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
  ShellUtil::ShortcutProperties test_properties_;

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
  scoped_ptr<base::ScopedPathOverride> start_menu_override_;
  scoped_ptr<base::ScopedPathOverride> common_start_menu_override_;

  base::FilePath chrome_exe_;
  base::FilePath manganese_exe_;
  base::FilePath iron_exe_;
  base::FilePath other_ico_;
};

}  // namespace

TEST_F(ShellUtilShortcutTest, GetShortcutPath) {
  base::FilePath path;

  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_user_desktop_.path(), path);

  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                             ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_common_desktop_.path(), path);

  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, dist_,
                             ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_user_quick_launch_.path(), path);

  base::string16 start_menu_subfolder =
      dist_->GetStartMenuShortcutSubfolder(
          BrowserDistribution::SUBFOLDER_CHROME);
  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                             dist_, ShellUtil::CURRENT_USER, &path);
  EXPECT_EQ(fake_start_menu_.path().Append(start_menu_subfolder),
            path);

  ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                             dist_, ShellUtil::SYSTEM_LEVEL, &path);
  EXPECT_EQ(fake_common_start_menu_.path().Append(start_menu_subfolder),
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
  test_properties_.set_shortcut_name(L"Bobo le shortcut");
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                         dist_, test_properties_);
}

TEST_F(ShellUtilShortcutTest, ReplaceSystemLevelDesktopShortcut) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  ShellUtil::ShortcutProperties new_properties(ShellUtil::SYSTEM_LEVEL);
  product_->AddDefaultShortcutProperties(chrome_exe_, &new_properties);
  new_properties.set_description(L"New description");
  new_properties.set_arguments(L"--new-arguments");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                  dist_, new_properties,
                  ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING));

  // Expect the properties set in |new_properties| to be set as above and
  // properties that don't have a default value to be set back to their default
  // (as validated in ValidateChromeShortcut()) or unset if they don't .
  ShellUtil::ShortcutProperties expected_properties(new_properties);
  expected_properties.set_dual_mode(false);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, UpdateQuickLaunchShortcutArguments) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                  dist_, test_properties_,
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
  ShellUtil::ShortcutProperties expected_properties(test_properties_);
  expected_properties.set_arguments(updated_properties.arguments);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, UpdateAddDualModeToStartMenuShortcut) {
  ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
  product_->AddDefaultShortcutProperties(chrome_exe_, &properties);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR, dist_,
                  properties, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  ShellUtil::ShortcutProperties added_properties(ShellUtil::CURRENT_USER);
  added_properties.set_dual_mode(true);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR, dist_,
                  added_properties, ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING));

  ShellUtil::ShortcutProperties expected_properties(properties);
  expected_properties.set_dual_mode(true);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                         dist_, expected_properties);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevel) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         test_properties_);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevelWithSystemLevelPresent) {
  base::string16 shortcut_name(
      dist_->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME) +
      installer::kLnkExt);

  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(
      fake_common_desktop_.path().Append(shortcut_name)));

  test_properties_.level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ASSERT_FALSE(base::PathExists(
      fake_user_desktop_.path().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevelStartMenu) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                         dist_, test_properties_);
}

TEST_F(ShellUtilShortcutTest, CreateAlwaysUserWithSystemLevelPresent) {
  base::string16 shortcut_name(
      dist_->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME) +
      installer::kLnkExt);

  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(
      fake_common_desktop_.path().Append(shortcut_name)));

  test_properties_.level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(
      fake_user_desktop_.path().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, RemoveChromeShortcut) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::CURRENT_USER,
      chrome_exe_));
  ASSERT_FALSE(base::PathExists(shortcut_path));
  ASSERT_TRUE(base::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveSystemLevelChromeShortcut) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::SYSTEM_LEVEL,
      chrome_exe_));
  ASSERT_FALSE(base::PathExists(shortcut_path));
  ASSERT_TRUE(base::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveMultipleChromeShortcuts) {
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));

  // Shortcut 2: targets "chrome.exe"; has arguments; icon set to "other.ico".
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  test_properties_.set_icon(other_ico_, 0);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));

  // Shortcut 3: targets "iron.exe"; has arguments; icon set to "chrome.exe".
  test_properties_.set_shortcut_name(L"Iron 3");
  test_properties_.set_target(iron_exe_);
  test_properties_.set_icon(chrome_exe_, 3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));

  // Remove shortcuts that target "chrome.exe".
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::CURRENT_USER,
      chrome_exe_));
  ASSERT_FALSE(base::PathExists(shortcut1_path));
  ASSERT_FALSE(base::PathExists(shortcut2_path));
  ASSERT_TRUE(base::PathExists(shortcut3_path));
  ASSERT_TRUE(base::PathExists(shortcut1_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RetargetShortcutsWithArgs) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_)));

  base::FilePath new_exe = manganese_exe_;
  // Relies on the fact that |test_properties_| has non-empty arguments.
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::CURRENT_USER,
      chrome_exe_, new_exe));

  ShellUtil::ShortcutProperties expected_properties(test_properties_);
  expected_properties.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, RetargetSystemLevelChromeShortcutsWithArgs) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_)));

  base::FilePath new_exe = manganese_exe_;
  // Relies on the fact that |test_properties_| has non-empty arguments.
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::SYSTEM_LEVEL,
      chrome_exe_, new_exe));

  ShellUtil::ShortcutProperties expected_properties(test_properties_);
  expected_properties.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, RetargetChromeShortcutsWithArgsEmpty) {
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties1(test_properties_);

  // Shortcut 2: targets "chrome.exe"; has arguments.
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties2(test_properties_);

  // Retarget shortcuts: replace "chrome.exe" with "manganese.exe". Only
  // shortcuts with non-empty arguments (i.e., shortcut 2) gets updated.
  base::FilePath new_exe = manganese_exe_;
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::CURRENT_USER,
      chrome_exe_, new_exe));

  // Verify shortcut 1: no change.
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties1);

  // Verify shortcut 2: target => "manganese.exe".
  expected_properties2.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties2);
}

TEST_F(ShellUtilShortcutTest, RetargetChromeShortcutsWithArgsIcon) {
  const int kTestIconIndex1 = 3;
  const int kTestIconIndex2 = 5;
  const int kTestIconIndex3 = 8;

  // Shortcut 1: targets "chrome.exe"; icon same as target.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_icon(test_properties_.target, kTestIconIndex1);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties1(test_properties_);

  // Shortcut 2: targets "chrome.exe"; icon set to "other.ico".
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_icon(other_ico_, kTestIconIndex2);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties2(test_properties_);

  // Shortcut 3: targets "iron.exe"; icon set to "chrome.exe".
  test_properties_.set_target(iron_exe_);
  test_properties_.set_shortcut_name(L"Iron 3");
  test_properties_.set_icon(chrome_exe_, kTestIconIndex3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties3(test_properties_);

  // Retarget shortcuts: replace "chrome.exe" with "manganese.exe".
  // Relies on the fact that |test_properties_| has non-empty arguments.
  base::FilePath new_exe = manganese_exe_;
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::CURRENT_USER,
      chrome_exe_, new_exe));

  // Verify shortcut 1: target & icon => "manganese.exe", kept same icon index.
  expected_properties1.set_target(new_exe);
  expected_properties1.set_icon(new_exe, kTestIconIndex1);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties1);

  // Verify shortcut 2: target => "manganese.exe", kept icon.
  expected_properties2.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties2);

  // Verify shortcut 3: no change, since target doesn't match.
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties3);
}

TEST_F(ShellUtilShortcutTest, ClearShortcutArguments) {
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));
  ShellUtil::ShortcutProperties expected_properties1(test_properties_);

  // Shortcut 2: targets "chrome.exe"; has 1 whitelisted argument.
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));
  ShellUtil::ShortcutProperties expected_properties2(test_properties_);

  // Shortcut 3: targets "chrome.exe"; has an unknown argument.
  test_properties_.set_shortcut_name(L"Chrome 3");
  test_properties_.set_arguments(L"foo.com");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));
  ShellUtil::ShortcutProperties expected_properties3(test_properties_);

  // Shortcut 4: targets "chrome.exe"; has both unknown and known arguments.
  test_properties_.set_shortcut_name(L"Chrome 4");
  test_properties_.set_arguments(L"foo.com --show-app-list");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut4_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut4_path));
  ShellUtil::ShortcutProperties expected_properties4(test_properties_);

  // List the shortcuts.
  std::vector<std::pair<base::FilePath, base::string16> > shortcuts;
  EXPECT_TRUE(ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP,
      dist_,
      ShellUtil::CURRENT_USER,
      chrome_exe_,
      false,
      NULL,
      &shortcuts));
  ASSERT_EQ(2u, shortcuts.size());
  std::pair<base::FilePath, base::string16> shortcut3 =
      shortcuts[0].first == shortcut3_path ? shortcuts[0] : shortcuts[1];
  std::pair<base::FilePath, base::string16> shortcut4 =
      shortcuts[0].first == shortcut4_path ? shortcuts[0] : shortcuts[1];
  EXPECT_EQ(shortcut3_path, shortcut3.first);
  EXPECT_EQ(L"foo.com", shortcut3.second);
  EXPECT_EQ(shortcut4_path, shortcut4.first);
  EXPECT_EQ(L"foo.com --show-app-list", shortcut4.second);

  // Clear shortcuts.
  shortcuts.clear();
  EXPECT_TRUE(ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP,
      dist_,
      ShellUtil::CURRENT_USER,
      chrome_exe_,
      true,
      NULL,
      &shortcuts));
  ASSERT_EQ(2u, shortcuts.size());
  shortcut3 = shortcuts[0].first == shortcut3_path ? shortcuts[0] :
                                                     shortcuts[1];
  shortcut4 = shortcuts[0].first == shortcut4_path ? shortcuts[0] :
                                                     shortcuts[1];
  EXPECT_EQ(shortcut3_path, shortcut3.first);
  EXPECT_EQ(L"foo.com", shortcut3.second);
  EXPECT_EQ(shortcut4_path, shortcut4.first);
  EXPECT_EQ(L"foo.com --show-app-list", shortcut4.second);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties1);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties2);
  expected_properties3.set_arguments(base::string16());
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties3);
  expected_properties4.set_arguments(L"--show-app-list");
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_,
                         expected_properties4);
}

TEST_F(ShellUtilShortcutTest, CreateMultipleStartMenuShortcutsAndRemoveFolder) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  test_properties_.set_shortcut_name(L"A second shortcut");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  base::FilePath chrome_shortcut_folder(
      fake_start_menu_.path().Append(
          dist_->GetStartMenuShortcutSubfolder(
              BrowserDistribution::SUBFOLDER_CHROME)));
  base::FilePath chrome_apps_shortcut_folder(
      fake_start_menu_.path().Append(
          dist_->GetStartMenuShortcutSubfolder(
              BrowserDistribution::SUBFOLDER_APPS)));

  base::FileEnumerator chrome_file_counter(chrome_shortcut_folder, false,
                                           base::FileEnumerator::FILES);
  int count = 0;
  while (!chrome_file_counter.Next().empty())
    ++count;
  EXPECT_EQ(2, count);

  base::FileEnumerator chrome_apps_file_counter(chrome_apps_shortcut_folder,
                                                false,
                                                base::FileEnumerator::FILES);
  count = 0;
  while (!chrome_apps_file_counter.Next().empty())
    ++count;
  EXPECT_EQ(2, count);

  ASSERT_TRUE(base::PathExists(chrome_shortcut_folder));
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR, dist_,
      ShellUtil::CURRENT_USER, chrome_exe_));
  ASSERT_FALSE(base::PathExists(chrome_shortcut_folder));

  ASSERT_TRUE(base::PathExists(chrome_apps_shortcut_folder));
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR, dist_,
      ShellUtil::CURRENT_USER, chrome_exe_));
  ASSERT_FALSE(base::PathExists(chrome_apps_shortcut_folder));
}

TEST_F(ShellUtilShortcutTest,
       DeleteStartMenuRootShortcutWithoutRemovingFolder) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT,
                  dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  base::string16 shortcut_name(
      dist_->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME) +
      installer::kLnkExt);
  base::FilePath shortcut_path(
      fake_start_menu_.path().Append(shortcut_name));

  ASSERT_TRUE(base::PathExists(fake_start_menu_.path()));
  ASSERT_TRUE(base::PathExists(shortcut_path));
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, dist_,
      ShellUtil::CURRENT_USER, chrome_exe_));
  // The shortcut should be removed but the "Start Menu" root directory should
  // remain.
  ASSERT_TRUE(base::PathExists(fake_start_menu_.path()));
  ASSERT_FALSE(base::PathExists(shortcut_path));
}

TEST_F(ShellUtilShortcutTest, DontRemoveChromeShortcutIfPointsToAnotherChrome) {
  base::ScopedTempDir other_exe_dir;
  ASSERT_TRUE(other_exe_dir.CreateUniqueTempDir());
  base::FilePath other_chrome_exe =
      other_exe_dir.path().Append(installer::kChromeExe);
  EXPECT_EQ(0, base::WriteFile(other_chrome_exe, "", 0));

  test_properties_.set_target(other_chrome_exe);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
                  ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, test_properties_,
                  ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  base::string16 shortcut_name(
      dist_->GetShortcutName(BrowserDistribution::SHORTCUT_CHROME) +
      installer::kLnkExt);
  base::FilePath shortcut_path(fake_user_desktop_.path().Append(shortcut_name));
  ASSERT_TRUE(base::PathExists(shortcut_path));

  // The shortcut shouldn't be removed as it was installed pointing to
  // |other_chrome_exe| and RemoveChromeShortcut() is being told that the
  // removed shortcut should point to |chrome_exe_|.
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, dist_, ShellUtil::CURRENT_USER,
      chrome_exe_));
  ASSERT_TRUE(base::PathExists(shortcut_path));
  ASSERT_TRUE(base::PathExists(shortcut_path.DirName()));
}

TEST(ShellUtilTest, BuildAppModelIdBasic) {
  std::vector<base::string16> components;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  const base::string16 base_app_id(dist->GetBaseAppId());
  components.push_back(base_app_id);
  ASSERT_EQ(base_app_id, ShellUtil::BuildAppModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdManySmall) {
  std::vector<base::string16> components;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  const base::string16 suffixed_app_id(dist->GetBaseAppId().append(L".gab"));
  components.push_back(suffixed_app_id);
  components.push_back(L"Default");
  components.push_back(L"Test");
  ASSERT_EQ(suffixed_app_id + L".Default.Test",
            ShellUtil::BuildAppModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdLongUsernameNormalProfile) {
  std::vector<base::string16> components;
  const base::string16 long_appname(
      L"Chrome.a_user_who_has_a_crazy_long_name_with_some_weird@symbols_in_it_"
      L"that_goes_over_64_characters");
  components.push_back(long_appname);
  components.push_back(L"Default");
  ASSERT_EQ(L"Chrome.a_user_wer_64_characters.Default",
            ShellUtil::BuildAppModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdLongEverything) {
  std::vector<base::string16> components;
  const base::string16 long_appname(L"Chrome.a_user_who_has_a_crazy_long_name_"
                                    L"with_some_weird@symbols_in_"
                                    L"it_" L"that_goes_over_64_characters");
  components.push_back(long_appname);
  components.push_back(
      L"A_crazy_profile_name_not_even_sure_whether_that_is_possible");
  const base::string16 constructed_app_id(
      ShellUtil::BuildAppModelId(components));
  ASSERT_LE(constructed_app_id.length(), installer::kMaxAppModelIdLength);
  ASSERT_EQ(L"Chrome.a_user_wer_64_characters.A_crazy_profilethat_is_possible",
            constructed_app_id);
}

TEST(ShellUtilTest, GetUserSpecificRegistrySuffix) {
  base::string16 suffix;
  ASSERT_TRUE(ShellUtil::GetUserSpecificRegistrySuffix(&suffix));
  ASSERT_TRUE(StartsWith(suffix, L".", true));
  ASSERT_EQ(27, suffix.length());
  ASSERT_TRUE(base::ContainsOnlyChars(suffix.substr(1),
                                      L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"));
}

TEST(ShellUtilTest, GetOldUserSpecificRegistrySuffix) {
  base::string16 suffix;
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

  const base::string16 expected[] = { L"", L"MY", L"MZXQ", L"MZXW6", L"MZXW6YQ",
                                L"MZXW6YTB", L"MZXW6YTBOI"};

  // Run the tests, with one more letter in the input every pass.
  for (int i = 0; i < arraysize(expected); ++i) {
    ASSERT_EQ(expected[i],
              ShellUtil::ByteArrayToBase32(test_array, i));
  }
}

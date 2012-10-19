// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/shortcut.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CreateVisualElementsManifestTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    // Create a temp directory for testing.
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

    version_ = Version("0.0.0.0");

    version_dir_ = test_dir_.path().AppendASCII(version_.GetString());
    ASSERT_TRUE(file_util::CreateDirectory(version_dir_));

    manifest_path_ =
        test_dir_.path().Append(installer::kVisualElementsManifest);
  }

  virtual void TearDown() OVERRIDE {
    // Clean up test directory manually so we can fail if it leaks.
    ASSERT_TRUE(test_dir_.Delete());
  }

  // The temporary directory used to contain the test operations.
  ScopedTempDir test_dir_;

  // A dummy version number used to create the version directory.
  Version version_;

  // The path to |test_dir_|\|version_|.
  FilePath version_dir_;

  // The path to VisualElementsManifest.xml.
  FilePath manifest_path_;
};

class InstallShortcutTest : public testing::Test {
 protected:
  // A mock installer state on which .system_install() will return
  // |system_install| and .target_path() will return |target_path|.
  class MockInstallerState : public installer::InstallerState {
   public:
    MockInstallerState(const FilePath& target_path,
                       bool system_install) : InstallerState() {
      target_path_ = target_path;
      set_level(system_install ? SYSTEM_LEVEL : USER_LEVEL);
    }
  };

  virtual void SetUp() OVERRIDE {
    EXPECT_EQ(S_OK, CoInitialize(NULL));

    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    ASSERT_TRUE(dist != NULL);
    product_.reset(new installer::Product(dist));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.path().Append(installer::kChromeExe);
    setup_exe_ = temp_dir_.path().Append(installer::kSetupExe);
    EXPECT_EQ(0, file_util::WriteFile(chrome_exe_, "", 0));
    EXPECT_EQ(0, file_util::WriteFile(setup_exe_, "", 0));

    mock_user_installer_state_.reset(
        new MockInstallerState(chrome_exe_.DirName(), false));
    mock_system_installer_state_.reset(
        new MockInstallerState(chrome_exe_.DirName(), true));

    expected_properties_.set_target(chrome_exe_);
    expected_properties_.set_dual_mode(true);

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

    string16 shortcut_name(dist->GetAppShortCutName() + installer::kLnkExt);
    string16 alternate_shortcut_name(
        dist->GetAlternateApplicationName() + installer::kLnkExt);

    user_desktop_shortcut_ =
        fake_user_desktop_.path().Append(shortcut_name);
    user_quick_launch_shortcut_ =
        fake_user_quick_launch_.path().Append(shortcut_name);
    user_start_menu_shortcut_ =
        fake_start_menu_.path().Append(dist->GetAppShortCutName())
        .Append(shortcut_name);
    system_desktop_shortcut_ =
        fake_common_desktop_.path().Append(shortcut_name);
    system_quick_launch_shortcut_ =
        fake_default_user_quick_launch_.path().Append(shortcut_name);
    system_start_menu_shortcut_ =
        fake_common_start_menu_.path().Append(dist->GetAppShortCutName())
        .Append(shortcut_name);
    user_alternate_desktop_shortcut_ =
        fake_user_desktop_.path().Append(alternate_shortcut_name);
  }

  virtual void TearDown() OVERRIDE {
    // Try to unpin potentially pinned shortcuts (although pinning isn't tested,
    // the call itself might still have pinned the Start Menu shortcuts).
    base::win::TaskbarUnpinShortcutLink(
        user_start_menu_shortcut_.value().c_str());
    base::win::TaskbarUnpinShortcutLink(
        system_start_menu_shortcut_.value().c_str());
    CoUninitialize();
  }

  base::win::ShortcutProperties expected_properties_;

  FilePath chrome_exe_;
  FilePath setup_exe_;
  scoped_ptr<installer::Product> product_;
  scoped_ptr<MockInstallerState> mock_user_installer_state_;
  scoped_ptr<MockInstallerState> mock_system_installer_state_;

  ScopedTempDir temp_dir_;
  ScopedTempDir fake_user_desktop_;
  ScopedTempDir fake_common_desktop_;
  ScopedTempDir fake_user_quick_launch_;
  ScopedTempDir fake_default_user_quick_launch_;
  ScopedTempDir fake_start_menu_;
  ScopedTempDir fake_common_start_menu_;
  scoped_ptr<base::ScopedPathOverride> user_desktop_override_;
  scoped_ptr<base::ScopedPathOverride> common_desktop_override_;
  scoped_ptr<base::ScopedPathOverride> user_quick_launch_override_;
  scoped_ptr<base::ScopedPathOverride> default_user_quick_launch_override_;
  scoped_ptr<base::ScopedPathOverride> start_menu_override_;
  scoped_ptr<base::ScopedPathOverride> common_start_menu_override_;

  FilePath user_desktop_shortcut_;
  FilePath user_quick_launch_shortcut_;
  FilePath user_start_menu_shortcut_;
  FilePath system_desktop_shortcut_;
  FilePath system_quick_launch_shortcut_;
  FilePath system_start_menu_shortcut_;
  FilePath user_alternate_desktop_shortcut_;
};

}  // namespace

// Test that VisualElementsManifest.xml is not created when VisualElements are
// not present.
TEST_F(CreateVisualElementsManifestTest, VisualElementsManifestNotCreated) {
  ASSERT_TRUE(
      installer::CreateVisualElementsManifest(test_dir_.path(), version_));
  ASSERT_FALSE(file_util::PathExists(manifest_path_));
}

// Test that VisualElementsManifest.xml is created with the correct content when
// VisualElements are present.
TEST_F(CreateVisualElementsManifestTest, VisualElementsManifestCreated) {
  ASSERT_TRUE(file_util::CreateDirectory(
      version_dir_.Append(installer::kVisualElements)));
  ASSERT_TRUE(
      installer::CreateVisualElementsManifest(test_dir_.path(), version_));
  ASSERT_TRUE(file_util::PathExists(manifest_path_));

  std::string read_manifest;
  ASSERT_TRUE(file_util::ReadFileToString(manifest_path_, &read_manifest));

  static const char kExpectedManifest[] =
      "<Application>\r\n"
      "  <VisualElements\r\n"
      "      DisplayName='Google Chrome'\r\n"
      "      Logo='0.0.0.0\\VisualElements\\Logo.png'\r\n"
      "      SmallLogo='0.0.0.0\\VisualElements\\SmallLogo.png'\r\n"
      "      ForegroundText='light'\r\n"
      "      BackgroundColor='white'>\r\n"
      "    <DefaultTile ShowName='allLogos'/>\r\n"
      "    <SplashScreen Image='0.0.0.0\\VisualElements\\splash-620x300.png'/>"
      "\r\n"
      "  </VisualElements>\r\n"
      "</Application>";

  ASSERT_STREQ(kExpectedManifest, read_manifest.c_str());
}

TEST_F(InstallShortcutTest, CreateAllShortcuts) {
  CreateOrUpdateShortcuts(*mock_user_installer_state_, setup_exe_, *product_,
                          installer::INSTALL_SHORTCUT_CREATE_ALL, false);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_, expected_properties_);
}

TEST_F(InstallShortcutTest, CreateAllShortcutsSystemLevel) {
  CreateOrUpdateShortcuts(*mock_system_installer_state_, setup_exe_, *product_,
                          installer::INSTALL_SHORTCUT_CREATE_ALL, false);
  base::win::ValidateShortcut(system_desktop_shortcut_, expected_properties_);
  base::win::ValidateShortcut(system_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(system_start_menu_shortcut_,
                              expected_properties_);
  // On system-level installs, the quick launch shortcut for the current user
  // should also be installed.
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
}

TEST_F(InstallShortcutTest, CreateAllShortcutsAlternateDesktopName) {
  CreateOrUpdateShortcuts(*mock_user_installer_state_, setup_exe_, *product_,
                          installer::INSTALL_SHORTCUT_CREATE_ALL, true);
  base::win::ValidateShortcut(user_alternate_desktop_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_properties_);
}

TEST_F(InstallShortcutTest, CreateMandatoryShortcuts) {
  CreateOrUpdateShortcuts(*mock_user_installer_state_, setup_exe_, *product_,
                          installer::INSTALL_SHORTCUT_CREATE_MANDATORY, false);
  ASSERT_FALSE(file_util::PathExists(user_desktop_shortcut_));
  ASSERT_FALSE(file_util::PathExists(user_quick_launch_shortcut_));
  base::win::ValidateShortcut(user_start_menu_shortcut_, expected_properties_);
}

TEST_F(InstallShortcutTest, ReplaceAll) {
  base::win::ShortcutProperties dummy_properties;
  FilePath dummy_target;
  ASSERT_TRUE(
      file_util::CreateTemporaryFileInDir(temp_dir_.path(), &dummy_target));
  dummy_properties.set_target(dummy_target);
  dummy_properties.set_working_dir(fake_user_desktop_.path());
  dummy_properties.set_arguments(L"--dummy --args");
  dummy_properties.set_app_id(L"El.Dummiest");

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_desktop_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_quick_launch_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::CreateDirectory(user_start_menu_shortcut_.DirName()));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_start_menu_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));

  CreateOrUpdateShortcuts(*mock_user_installer_state_, setup_exe_, *product_,
                          installer::INSTALL_SHORTCUT_REPLACE_EXISTING, false);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_properties_);
}

TEST_F(InstallShortcutTest, ReplaceExisting) {
  base::win::ShortcutProperties dummy_properties;
  FilePath dummy_target;
  ASSERT_TRUE(
      file_util::CreateTemporaryFileInDir(temp_dir_.path(), &dummy_target));
  dummy_properties.set_target(dummy_target);
  dummy_properties.set_working_dir(fake_user_desktop_.path());
  dummy_properties.set_arguments(L"--dummy --args");
  dummy_properties.set_app_id(L"El.Dummiest");

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_desktop_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(file_util::CreateDirectory(user_start_menu_shortcut_.DirName()));

  CreateOrUpdateShortcuts(*mock_user_installer_state_, setup_exe_, *product_,
                          installer::INSTALL_SHORTCUT_REPLACE_EXISTING, false);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  ASSERT_FALSE(file_util::PathExists(user_quick_launch_shortcut_));
  ASSERT_FALSE(file_util::PathExists(user_start_menu_shortcut_));
}

TEST(EscapeXmlAttributeValueTest, EscapeCrazyValue) {
  string16 val(L"This has 'crazy' \"chars\" && < and > signs.");
  static const wchar_t kExpectedEscapedVal[] =
      L"This has &apos;crazy&apos; \"chars\" &amp;&amp; &lt; and > signs.";
  installer::EscapeXmlAttributeValueInSingleQuotes(&val);
  ASSERT_STREQ(kExpectedEscapedVal, val.c_str());
}

TEST(EscapeXmlAttributeValueTest, DontEscapeNormalValue) {
  string16 val(L"Google Chrome");
  static const wchar_t kExpectedEscapedVal[] = L"Google Chrome";
  installer::EscapeXmlAttributeValueInSingleQuotes(&val);
  ASSERT_STREQ(kExpectedEscapedVal, val.c_str());
}

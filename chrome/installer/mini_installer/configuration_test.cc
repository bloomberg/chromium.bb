// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

using mini_installer::Configuration;

class TestConfiguration : public Configuration {
 public:
  explicit TestConfiguration(const wchar_t* command_line) : Configuration() {
    Initialize(command_line);
  }
 private:
  void Initialize(const wchar_t* command_line) {
    ASSERT_TRUE(InitializeFromCommandLine(command_line));
  }
};

// Test that the operation type is CLEANUP iff --cleanup is on the cmdline.
TEST(MiniInstallerConfigurationTest, Operation) {
  EXPECT_EQ(Configuration::INSTALL_PRODUCT,
            TestConfiguration(L"spam.exe").operation());
  EXPECT_EQ(Configuration::INSTALL_PRODUCT,
            TestConfiguration(L"spam.exe --clean").operation());
  EXPECT_EQ(Configuration::INSTALL_PRODUCT,
            TestConfiguration(L"spam.exe --cleanupthis").operation());

  EXPECT_EQ(Configuration::CLEANUP,
            TestConfiguration(L"spam.exe --cleanup").operation());
  EXPECT_EQ(Configuration::CLEANUP,
            TestConfiguration(L"spam.exe --cleanup now").operation());
}

TEST(MiniInstallerConfigurationTest, Program) {
  EXPECT_TRUE(NULL == mini_installer::Configuration().program());
  EXPECT_TRUE(std::wstring(L"spam.exe") ==
              TestConfiguration(L"spam.exe").program());
  EXPECT_TRUE(std::wstring(L"spam.exe") ==
              TestConfiguration(L"spam.exe --with args").program());
  EXPECT_TRUE(std::wstring(L"c:\\blaz\\spam.exe") ==
              TestConfiguration(L"c:\\blaz\\spam.exe --with args").program());
}

TEST(MiniInstallerConfigurationTest, ArgumentCount) {
  EXPECT_EQ(1, TestConfiguration(L"spam.exe").argument_count());
  EXPECT_EQ(2, TestConfiguration(L"spam.exe --foo").argument_count());
  EXPECT_EQ(3, TestConfiguration(L"spam.exe --foo --bar").argument_count());
}

TEST(MiniInstallerConfigurationTest, CommandLine) {
  static const wchar_t* const kCommandLines[] = {
    L"",
    L"spam.exe",
    L"spam.exe --foo",
  };
  for (size_t i = 0; i < arraysize(kCommandLines); ++i) {
    EXPECT_TRUE(std::wstring(kCommandLines[i]) ==
                TestConfiguration(kCommandLines[i]).command_line());
  }
}

TEST(MiniInstallerConfigurationTest, ChromeAppGuid) {
  EXPECT_TRUE(std::wstring(google_update::kAppGuid) ==
              TestConfiguration(L"spam.exe").chrome_app_guid());
  EXPECT_TRUE(std::wstring(google_update::kAppGuid) ==
              TestConfiguration(L"spam.exe --chrome").chrome_app_guid());
  EXPECT_TRUE(std::wstring(google_update::kAppGuid) ==
              TestConfiguration(L"spam.exe --multi-install --chrome")
                  .chrome_app_guid());
  EXPECT_TRUE(std::wstring(google_update::kAppGuid) ==
              TestConfiguration(L"spam.exe --chrome-frame").chrome_app_guid());
  EXPECT_TRUE(std::wstring(google_update::kSxSAppGuid) ==
              TestConfiguration(L"spam.exe --chrome-sxs").chrome_app_guid());
}

TEST(MiniInstallerConfigurationTest, HasChrome) {
  EXPECT_TRUE(TestConfiguration(L"spam.exe").has_chrome());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --chrome").has_chrome());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --multi-install --chrome")
                  .has_chrome());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --chrome-frame").has_chrome());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --multi-install").has_chrome());
}

TEST(MiniInstallerConfigurationTest, HasChromeFrame) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").has_chrome_frame());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --chrome").has_chrome_frame());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --multi-install --chrome")
                   .has_chrome_frame());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --chrome-frame").has_chrome_frame());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --multi-install --chrome-frame")
                  .has_chrome_frame());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --multi-install")
                   .has_chrome_frame());
}

TEST(MiniInstallerConfigurationTest, IsMultiInstall) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").is_multi_install());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --chrome").is_multi_install());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --multi-install --chrome")
                  .is_multi_install());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --chrome-frame")
                   .is_multi_install());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --multi-install --chrome-frame")
                  .is_multi_install());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --multi-install")
                  .is_multi_install());
}

TEST(MiniInstallerConfigurationTest, IsSystemLevel) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").is_system_level());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --chrome").is_system_level());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --system-level").is_system_level());
}

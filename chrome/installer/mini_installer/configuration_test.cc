// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/configuration.h"

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "base/environment.h"
#include "base/macros.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

namespace {

// A helper class to set the "GoogleUpdateIsMachine" environment variable.
class ScopedGoogleUpdateIsMachine {
 public:
  explicit ScopedGoogleUpdateIsMachine(bool value)
      : env_(base::Environment::Create()) {
    env_->SetVar("GoogleUpdateIsMachine", value ? "1" : "0");
  }

  ~ScopedGoogleUpdateIsMachine() {
    env_->UnSetVar("GoogleUpdateIsMachine");
  }

 private:
  std::unique_ptr<base::Environment> env_;
};

class TestConfiguration : public Configuration {
 public:
  explicit TestConfiguration(const wchar_t* command_line) {
    EXPECT_TRUE(ParseCommandLine(command_line));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConfiguration);
};

}  // namespace

class MiniInstallerConfigurationTest : public ::testing::Test {
 protected:
  MiniInstallerConfigurationTest() = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

 private:
  registry_util::RegistryOverrideManager registry_overrides_;

  DISALLOW_COPY_AND_ASSIGN(MiniInstallerConfigurationTest);
};

// Test that the operation type is CLEANUP iff --cleanup is on the cmdline.
TEST_F(MiniInstallerConfigurationTest, Operation) {
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

TEST_F(MiniInstallerConfigurationTest, Program) {
  EXPECT_TRUE(NULL == mini_installer::Configuration().program());
  EXPECT_TRUE(std::wstring(L"spam.exe") ==
              TestConfiguration(L"spam.exe").program());
  EXPECT_TRUE(std::wstring(L"spam.exe") ==
              TestConfiguration(L"spam.exe --with args").program());
  EXPECT_TRUE(std::wstring(L"c:\\blaz\\spam.exe") ==
              TestConfiguration(L"c:\\blaz\\spam.exe --with args").program());
}

TEST_F(MiniInstallerConfigurationTest, ArgumentCount) {
  EXPECT_EQ(1, TestConfiguration(L"spam.exe").argument_count());
  EXPECT_EQ(2, TestConfiguration(L"spam.exe --foo").argument_count());
  EXPECT_EQ(3, TestConfiguration(L"spam.exe --foo --bar").argument_count());
}

TEST_F(MiniInstallerConfigurationTest, CommandLine) {
  static const wchar_t* const kCommandLines[] = {
    L"",
    L"spam.exe",
    L"spam.exe --foo",
  };
  for (size_t i = 0; i < _countof(kCommandLines); ++i) {
    EXPECT_TRUE(std::wstring(kCommandLines[i]) ==
                TestConfiguration(kCommandLines[i]).command_line());
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(MiniInstallerConfigurationTest, ChromeAppGuid) {
  EXPECT_STREQ(google_update::kAppGuid,
               TestConfiguration(L"spam.exe").chrome_app_guid());
  EXPECT_STREQ(google_update::kBetaAppGuid,
               TestConfiguration(L"spam.exe --chrome-beta").chrome_app_guid());
  EXPECT_STREQ(google_update::kDevAppGuid,
               TestConfiguration(L"spam.exe --chrome-dev").chrome_app_guid());
  EXPECT_STREQ(google_update::kSxSAppGuid,
               TestConfiguration(L"spam.exe --chrome-sxs").chrome_app_guid());
}
#endif

TEST_F(MiniInstallerConfigurationTest, IsSystemLevel) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").is_system_level());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --chrome").is_system_level());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --system-level").is_system_level());

  {
    ScopedGoogleUpdateIsMachine env_setter(false);
    EXPECT_FALSE(TestConfiguration(L"spam.exe").is_system_level());
  }

  {
    ScopedGoogleUpdateIsMachine env_setter(true);
    EXPECT_TRUE(TestConfiguration(L"spam.exe").is_system_level());
  }
}

TEST_F(MiniInstallerConfigurationTest, HasInvalidSwitch) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").has_invalid_switch());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --chrome-frame")
                  .has_invalid_switch());
}

}  // namespace mini_installer

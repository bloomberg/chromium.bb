// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/disable_outdated_build_detector/disable_outdated_build_detector.h"

#include "base/environment.h"
#include "base/strings/string16.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/tools/disable_outdated_build_detector/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

enum class ExecutionMode {
  USER_LEVEL,
  SYSTEM_LEVEL_SWITCH,
  IS_MACHINE_ENV,
};

class DisableOutdatedBuildDetectorTest
    : public ::testing::TestWithParam<ExecutionMode> {
 protected:
  DisableOutdatedBuildDetectorTest()
      : chrome_distribution_(BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER)),
        binaries_distribution_(BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BINARIES)),
        execution_mode_(GetParam()),
        root_(execution_mode_ == ExecutionMode::USER_LEVEL
                  ? HKEY_CURRENT_USER
                  : HKEY_LOCAL_MACHINE) {
    registry_override_manager_.OverrideRegistry(root_);
    if (execution_mode_ == ExecutionMode::SYSTEM_LEVEL_SWITCH)
      command_line_ = L"--system-level";
    base::Environment::Create()->SetVar(
        "GoogleUpdateIsMachine",
        execution_mode_ == ExecutionMode::IS_MACHINE_ENV ? "1" : "0");
  }

  void FakeChrome(bool multi_install, const wchar_t* brand) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(root_, chrome_distribution_->GetStateKey().c_str(),
                         KEY_ALL_ACCESS | KEY_WOW64_32KEY));
    base::string16 uninstall_arguments(L"--uninstall");
    if (multi_install)
      uninstall_arguments += L"--chrome --multi-install";
    if (brand)
      ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"brand", brand));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"UninstallArguments",
                                            uninstall_arguments.c_str()));
    if (!multi_install)
      return;
    uninstall_arguments = L"--uninstall --multi-install";
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(root_, binaries_distribution_->GetStateKey().c_str(),
                         KEY_ALL_ACCESS | KEY_WOW64_32KEY));
    if (brand)
      ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"brand", brand));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"UninstallArguments",
                                            uninstall_arguments.c_str()));
  }

  bool HasBrand(BrowserDistribution* dist) {
    base::win::RegKey key(root_, dist->GetStateKey().c_str(),
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    return key.Valid() && key.HasValue(L"brand");
  }

  base::string16 ReadBrand(BrowserDistribution* dist) {
    base::win::RegKey key(root_, dist->GetStateKey().c_str(),
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    base::string16 brand;
    if (!key.Valid() || key.ReadValue(L"brand", &brand) != ERROR_SUCCESS)
      return base::string16();
    return brand;
  }

  // Verifies that |result|, |exit_code|, and |detail| are found in Chrome's
  // ClientStateKey in the InstallerResult, InstallerError, and
  // InstallerExtraCode1 values, respectively.
  void ExpectResult(InstallerResult result,
                    ExitCode exit_code,
                    uint32_t detail) {
    base::win::RegKey key(root_, chrome_distribution_->GetStateKey().c_str(),
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    ASSERT_TRUE(key.Valid());
    DWORD value = 0;
    ASSERT_EQ(ERROR_SUCCESS,
              key.ReadValueDW(installer::kInstallerResult, &value));
    EXPECT_EQ(result, static_cast<InstallerResult>(value));
    ASSERT_EQ(ERROR_SUCCESS,
              key.ReadValueDW(installer::kInstallerError, &value));
    EXPECT_EQ(exit_code, static_cast<ExitCode>(value));
    if (detail) {
      ASSERT_EQ(ERROR_SUCCESS,
                key.ReadValueDW(installer::kInstallerExtraCode1, &value));
      EXPECT_EQ(detail, value);
    } else {
      EXPECT_FALSE(key.HasValue(installer::kInstallerExtraCode1));
    }
  }

  const wchar_t* command_line() const { return command_line_.c_str(); }

  std::wstring command_line_;
  BrowserDistribution* chrome_distribution_;
  BrowserDistribution* binaries_distribution_;

 private:
  const ExecutionMode execution_mode_;
  const HKEY root_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_P(DisableOutdatedBuildDetectorTest, NoChrome) {
  EXPECT_EQ(ExitCode::NO_CHROME, DisableOutdatedBuildDetector(command_line()));
}

TEST_P(DisableOutdatedBuildDetectorTest, SingleUnbrandedChrome) {
  // Fake single-install Chrome's ClientState key with no brand.
  FakeChrome(false /* single-install */, nullptr);

  // Switch the brand.
  EXPECT_EQ(ExitCode::NON_ORGANIC_BRAND,
            DisableOutdatedBuildDetector(command_line()));
  ExpectResult(InstallerResult::FAILED_CUSTOM_ERROR,
               ExitCode::NON_ORGANIC_BRAND, ERROR_FILE_NOT_FOUND);

  // Verify that there is still no brand.
  EXPECT_FALSE(HasBrand(chrome_distribution_));

  // And the binaries' ClientState key should not have been created.
  EXPECT_FALSE(HasBrand(binaries_distribution_));
}

TEST_P(DisableOutdatedBuildDetectorTest, SingleOrganicChrome) {
  // Fake single-install Chrome's ClientState key with an organic brand.
  FakeChrome(false /* single-install */, L"GGLS");

  // Switch the brand.
  EXPECT_EQ(ExitCode::CHROME_BRAND_UPDATED,
            DisableOutdatedBuildDetector(command_line()));
  ExpectResult(InstallerResult::FAILED_CUSTOM_ERROR,
               ExitCode::CHROME_BRAND_UPDATED, 0);

  // Verify the new brand.
  EXPECT_STREQ(L"AOHY", ReadBrand(chrome_distribution_).c_str());

  // And the binaries' ClientState key should not have been created.
  EXPECT_FALSE(HasBrand(binaries_distribution_));
}

TEST_P(DisableOutdatedBuildDetectorTest, SingleInOrganicChrome) {
  static const wchar_t kBlorBrand[] = L"BLOR";

  // Fake single-install Chrome's ClientState key with an inorganic brand.
  FakeChrome(false /* single-install */, kBlorBrand);

  // Switch the brand.
  EXPECT_EQ(ExitCode::NON_ORGANIC_BRAND,
            DisableOutdatedBuildDetector(command_line()));
  ExpectResult(InstallerResult::FAILED_CUSTOM_ERROR,
               ExitCode::NON_ORGANIC_BRAND, 0);

  // Verify that the brand is unchanged.
  EXPECT_STREQ(kBlorBrand, ReadBrand(chrome_distribution_).c_str());

  // And the binaries' ClientState key should not have been created.
  EXPECT_FALSE(HasBrand(binaries_distribution_));
}

TEST_P(DisableOutdatedBuildDetectorTest, MultiOrganicChrome) {
  // Fake multi-install Chrome's ClientState key with an organic brand.
  FakeChrome(true /* multi-install */, L"GGLS");

  // Switch the brand.
  EXPECT_EQ(ExitCode::BOTH_BRANDS_UPDATED,
            DisableOutdatedBuildDetector(command_line()));
  ExpectResult(InstallerResult::FAILED_CUSTOM_ERROR,
               ExitCode::BOTH_BRANDS_UPDATED, 0);

  // Verify the new brand in Chrome and the binaries.
  EXPECT_STREQ(L"AOHY", ReadBrand(chrome_distribution_).c_str());
  EXPECT_STREQ(L"AOHY", ReadBrand(binaries_distribution_).c_str());
}

TEST_P(DisableOutdatedBuildDetectorTest, MultiInOrganicChrome) {
  static const wchar_t kBlorBrand[] = L"BLOR";

  // Fake multi-install Chrome's ClientState key with an inorganic brand.
  FakeChrome(true /* multi-install */, kBlorBrand);

  // Switch the brand.
  EXPECT_EQ(ExitCode::NON_ORGANIC_BRAND,
            DisableOutdatedBuildDetector(command_line()));
  ExpectResult(InstallerResult::FAILED_CUSTOM_ERROR,
               ExitCode::NON_ORGANIC_BRAND, 0);

  // Verify that the brand is unchanged in both apps.
  EXPECT_STREQ(kBlorBrand, ReadBrand(chrome_distribution_).c_str());
  EXPECT_STREQ(kBlorBrand, ReadBrand(binaries_distribution_).c_str());
}

INSTANTIATE_TEST_CASE_P(UserLevel,
                        DisableOutdatedBuildDetectorTest,
                        ::testing::Values(ExecutionMode::USER_LEVEL));
INSTANTIATE_TEST_CASE_P(SystemLevelSwitch,
                        DisableOutdatedBuildDetectorTest,
                        ::testing::Values(ExecutionMode::SYSTEM_LEVEL_SWITCH));
INSTANTIATE_TEST_CASE_P(IsMachineEnv,
                        DisableOutdatedBuildDetectorTest,
                        ::testing::Values(ExecutionMode::IS_MACHINE_ENV));

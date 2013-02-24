// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_validator.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/mini_installer_test/installer_path_provider.h"
#include "chrome/test/mini_installer_test/installer_test_util.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome/test/mini_installer_test/switch_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using installer::InstallationValidator;
using installer_test::InstallerPathProvider;
using installer_test::SwitchBuilder;

namespace {

class MiniInstallTest : public testing::Test {
 public:
  virtual void SetUp() {
    std::vector<installer_test::InstalledProduct> installed;
    if (installer_test::GetInstalledProducts(&installed)) {
      ASSERT_TRUE(installer_test::UninstallAll());
    }
  }

  virtual void TearDown() {
    installer_test::UninstallAll();
  }

 protected:
  static void SetUpTestCase() {
    std::string build_under_test;
    const CommandLine* cmd = CommandLine::ForCurrentProcess();
    build_under_test = cmd->GetSwitchValueASCII(switches::kInstallerTestBuild);
    if (build_under_test.empty())
      provider_ = new InstallerPathProvider();
    else
      provider_ = new InstallerPathProvider(build_under_test);
    ASSERT_FALSE(provider_->GetCurrentBuild().empty());
    ASSERT_TRUE(provider_->GetFullInstaller(&full_installer_));
    ASSERT_TRUE(provider_->GetPreviousInstaller(&previous_installer_));
    ASSERT_TRUE(provider_->GetDiffInstaller(&diff_installer_));
    ASSERT_TRUE(
        provider_->GetSignedStandaloneInstaller(&standalone_installer_));
    ASSERT_TRUE(provider_->GetMiniInstaller(&mini_installer_));
  }

  static void TearDownTestCase() {
    delete provider_;
    provider_ = NULL;
  }

  static InstallerPathProvider* provider_;
  static base::FilePath full_installer_;
  static base::FilePath previous_installer_;
  static base::FilePath diff_installer_;
  static base::FilePath standalone_installer_;
  static base::FilePath mini_installer_;
};

InstallerPathProvider* MiniInstallTest::provider_;
base::FilePath MiniInstallTest::full_installer_;
base::FilePath MiniInstallTest::previous_installer_;
base::FilePath MiniInstallTest::diff_installer_;
base::FilePath MiniInstallTest::standalone_installer_;
base::FilePath MiniInstallTest::mini_installer_;

}  // namespace

#if defined(GOOGLE_CHROME_BUILD)
// Could use a parameterized gtest to slim down this list of tests, but since
// these tests will often be run manually, don't want to have obscure test
// names.

// Install full installer at user level.
TEST_F(MiniInstallTest, FullInstallerUser) {
  ASSERT_TRUE(installer_test::Install(
      full_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

// Install full installer at system level.
TEST_F(MiniInstallTest, FullInstallerSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

// Overinstall full installer.
TEST_F(MiniInstallTest, FullOverPreviousFullUser) {
  ASSERT_TRUE(installer_test::Install(
      previous_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(
      full_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, FullOverPreviousFullSys) {
  ASSERT_TRUE(installer_test::Install(previous_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, FreshChromeFrameUser) {
  ASSERT_TRUE(installer_test::Install(
      full_installer_, SwitchBuilder().AddChromeFrame()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      false,
      InstallationValidator::CHROME_FRAME_SINGLE,
      provider_->GetCurrentBuild()));
}

// Overinstall full Chrome Frame installer while IE browser is running.
TEST_F(MiniInstallTest, FullFrameOverPreviousFullIERunningSys) {
  installer_test::LaunchIE("http://www.google.com");
  ASSERT_TRUE(installer_test::Install(previous_installer_,
      SwitchBuilder().AddChromeFrame().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_SINGLE,
      provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChromeFrame().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_SINGLE,
      provider_->GetCurrentBuild()));
}

// Overinstall diff installer.
TEST_F(MiniInstallTest, DiffOverPreviousFullUser) {
  ASSERT_TRUE(installer_test::Install(
      previous_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(
      diff_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, DiffOverPreviousFullSys) {
  ASSERT_TRUE(installer_test::Install(previous_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(diff_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

// Overinstall diff Chrome Frame installer while IE browser is running.
TEST_F(MiniInstallTest, DiffFrameOverPreviousFullIERunningSys) {
  installer_test::LaunchIE("http://www.google.com");
  ASSERT_TRUE(installer_test::Install(previous_installer_,
    SwitchBuilder().AddChromeFrame().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_SINGLE,
      provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(diff_installer_,
      SwitchBuilder().AddChromeFrame().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_SINGLE,
      provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, InstallChromeMultiOverChromeSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
    SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
    InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
  ASSERT_TRUE(installer_test::Install(full_installer_,
    SwitchBuilder().AddChrome().AddSystemInstall().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
    InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
}

// Repair version folder.
TEST_F(MiniInstallTest, RepairFolderOnFullUser) {
  ASSERT_TRUE(installer_test::Install(
      full_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
  base::CleanupProcesses(installer::kChromeExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  ASSERT_TRUE(installer_test::DeleteInstallDirectory(
      false, // system level
      InstallationValidator::CHROME_SINGLE));
  ASSERT_TRUE(installer_test::Install(
      full_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, RepairFolderOnFullSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
  base::CleanupProcesses(installer::kChromeExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  ASSERT_TRUE(installer_test::DeleteInstallDirectory(
      true, // system level
      InstallationValidator::CHROME_SINGLE));
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

// Repair registry.
TEST_F(MiniInstallTest, RepairRegistryOnFullUser) {
  ASSERT_TRUE(installer_test::Install(
      full_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
  base::CleanupProcesses(installer::kChromeExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  ASSERT_TRUE(installer_test::DeleteRegistryKey(
      false, // system level
      InstallationValidator::CHROME_SINGLE));
  ASSERT_TRUE(
      installer_test::Install(full_installer_, SwitchBuilder().AddChrome()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, RepairRegistryOnFullSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
  ASSERT_TRUE(installer_test::DeleteRegistryKey(
      true, // system level
      InstallationValidator::CHROME_SINGLE));
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_SINGLE, provider_->GetCurrentBuild()));
}

// Run full Chrome Frame install then uninstall it while IE browser is running.
TEST_F(MiniInstallTest, FullInstallAndUnInstallChromeFrameWithIERunning) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChromeFrame().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_SINGLE,
      provider_->GetCurrentBuild()));
  // Launch IE and let TearDown step perform uninstall.
  installer_test::LaunchIE("http://www.google.com");
}

// Install standalone.
TEST_F(MiniInstallTest, InstallStandaloneUser) {
  ASSERT_TRUE(installer_test::Install(standalone_installer_));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
}

// This test doesn't make sense. Disabling for now.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerOverChromeMetaInstallerTest) {
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
    DISABLED_InstallLatestStableFullInstallerOverChromeMetaInstaller) {
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestDevFullInstallerOverChromeMetaInstallerTest) {
}

TEST_F(MiniInstallTest,
    InstallChromeUsingMultiInstallUser) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest,
    InstallChromeUsingMultiInstallSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, InstallChromeAndChromeFrameMultiInstallUser) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddChromeFrame().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      false,
      InstallationValidator::CHROME_FRAME_MULTI_CHROME_MULTI,
      provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, InstallChromeAndChromeFrameMultiInstallSys) {
  ASSERT_TRUE(installer_test::Install(
      full_installer_, SwitchBuilder().AddChrome()
      .AddChromeFrame().AddMultiInstall().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_MULTI_CHROME_MULTI,
      provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest,
    InstallChromeAndChromeFrameReadyModeUser) {
  ASSERT_TRUE(
      installer_test::Install(full_installer_,SwitchBuilder().AddChrome()
      .AddChromeFrame().AddMultiInstall().AddReadyMode()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      false,
      InstallationValidator::CHROME_FRAME_READY_MODE_CHROME_MULTI,
      provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest,
    InstallChromeAndChromeFrameReadyModeSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddChromeFrame().AddMultiInstall()
      .AddReadyMode().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_READY_MODE_CHROME_MULTI,
      provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, InstallChromeFrameUsingMultiInstallUser) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChromeFrame().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      false,
      InstallationValidator::CHROME_FRAME_MULTI,
      provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, InstallChromeFrameUsingMultiInstallSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChromeFrame().AddMultiInstall().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_MULTI,
      provider_->GetCurrentBuild()));
}

// Chrome Frame is in use while Chrome is install.
TEST_F(MiniInstallTest, InstallChromeWithExistingChromeFrameMultiInstallUser) {
  ASSERT_TRUE(installer_test::Install(previous_installer_,
      SwitchBuilder().AddChromeFrame().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      false,
      InstallationValidator::CHROME_FRAME_MULTI,
      provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      false,
      InstallationValidator::CHROME_FRAME_MULTI_CHROME_MULTI,
      provider_->GetCurrentBuild()));
}

// Chrome Frame is in use while Chrome is install.
TEST_F(MiniInstallTest, InstallChromeWithExistingChromeFrameMultiInstallSys) {
  ASSERT_TRUE(installer_test::Install(previous_installer_,
      SwitchBuilder().AddChromeFrame().AddMultiInstall().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(
      true,
      InstallationValidator::CHROME_FRAME_MULTI,
      provider_->GetPreviousBuild()));
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, OverInstallChromeWhenInUseUser) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
  installer_test::LaunchChrome(false, false);
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(false,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
}

TEST_F(MiniInstallTest, OverInstallChromeWhenInUseSys) {
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
  installer_test::LaunchChrome(false, true);
  ASSERT_TRUE(installer_test::Install(full_installer_,
      SwitchBuilder().AddChrome().AddMultiInstall().AddSystemInstall()));
  ASSERT_TRUE(installer_test::ValidateInstall(true,
      InstallationValidator::CHROME_MULTI, provider_->GetCurrentBuild()));
}

#endif

TEST(GenericInstallTest, MiniInstallTestValidWindowsVersion) {
  // We run the tests on all supported OSes.
  // Make sure the code agrees.
  EXPECT_TRUE(InstallUtil::IsOSSupported());
}


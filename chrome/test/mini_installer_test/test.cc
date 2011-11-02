// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/mini_installer_test/chrome_mini_installer.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"


// Although the C++ style guide disallows use of namespace directive, use
// here because this is not only a .cc file, but also a test.
using namespace mini_installer_constants;  // NOLINT

namespace {
class MiniInstallTest : public testing::Test {
 public:
  MiniInstallTest() {
    std::string build;
    const CommandLine* cmd = CommandLine::ForCurrentProcess();
    build = cmd->GetSwitchValueASCII(switches::kInstallerTestBuild);
    chrome_user_installer_.reset(
        new ChromeMiniInstaller(false, false, build));
    chrome_sys_installer_.reset(new ChromeMiniInstaller(true, false, build));
    cf_user_installer_.reset(new ChromeMiniInstaller(false, true, build));
    cf_sys_installer_.reset(new ChromeMiniInstaller(true, true, build));
  }

  virtual void TearDown() {
    chrome_user_installer_->UnInstall();
    chrome_sys_installer_->UnInstall();
    cf_user_installer_->UnInstall();
    cf_sys_installer_->UnInstall();
  }

 protected:
  scoped_ptr<ChromeMiniInstaller> chrome_user_installer_;
  scoped_ptr<ChromeMiniInstaller> chrome_sys_installer_;
  scoped_ptr<ChromeMiniInstaller> cf_user_installer_;
  scoped_ptr<ChromeMiniInstaller> cf_sys_installer_;
};
}  // namespace

#if defined(GOOGLE_CHROME_BUILD)
// Could use a parameterized gtest to slim down this list of tests, but since
// these tests will often be run manually, don't want to have obscure test
// names.

// Install full installer at system level.
TEST_F(MiniInstallTest, FullInstallerSys) {
  chrome_sys_installer_->InstallFullInstaller(false);
}

// Install full installer at user level.
TEST_F(MiniInstallTest, FullInstallerUser) {
  chrome_user_installer_->InstallFullInstaller(false);
}
// Overinstall full installer.
TEST_F(MiniInstallTest, FullOverPreviousFullUser) {
  chrome_user_installer_->OverInstallOnFullInstaller(kFullInstall, false);
}
TEST_F(MiniInstallTest, FullOverPreviousFullSys) {
  chrome_sys_installer_->OverInstallOnFullInstaller(kFullInstall, false);
}

// Overinstall full Chrome Frame installer while IE browser is running.
TEST_F(MiniInstallTest, FullFrameOverPreviousFullIERunningSys) {
  cf_sys_installer_->OverInstallOnFullInstaller(kFullInstall, true);
}

// Overinstall diff installer.
TEST_F(MiniInstallTest, DiffOverPreviousFullUser) {
  chrome_user_installer_->OverInstallOnFullInstaller(kDiffInstall, false);
}

TEST_F(MiniInstallTest, DiffOverPreviousFullSys) {
  chrome_sys_installer_->OverInstallOnFullInstaller(kDiffInstall, false);
}

// Overinstall diff Chrome Frame installer while IE browser is running.
TEST_F(MiniInstallTest, DiffFrameOverPreviousFullIERunningSys) {
  cf_sys_installer_->OverInstallOnFullInstaller(kDiffInstall, true);
}

// Repair version folder.
TEST_F(MiniInstallTest, RepairFolderOnFullUser) {
  chrome_user_installer_->Repair(ChromeMiniInstaller::VERSION_FOLDER);
}

TEST_F(MiniInstallTest, RepairFolderOnFullSys) {
  chrome_sys_installer_->Repair(ChromeMiniInstaller::VERSION_FOLDER);
}

// Repair registry.
TEST_F(MiniInstallTest, RepairRegistryOnFullUser) {
  chrome_user_installer_->Repair(ChromeMiniInstaller::REGISTRY);
}
TEST_F(MiniInstallTest, RepairRegistryOnFullSys) {
  chrome_sys_installer_->Repair(ChromeMiniInstaller::REGISTRY);
}

// Run full Chrome Frame install then uninstall it while IE browser is running.
TEST_F(MiniInstallTest, FullInstallAndUnInstallChromeFrameWithIERunning) {
  cf_sys_installer_->InstallFullInstaller(false);
  cf_sys_installer_->UnInstallChromeFrameWithIERunning();
}

// Install standalone.
TEST_F(MiniInstallTest, InstallStandaloneUser) {
  chrome_user_installer_->InstallStandaloneInstaller();
}

// This test doesn't make sense. Disabling for now.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerOverChromeMetaInstallerTest) {
  chrome_user_installer_->OverInstall();
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
    DISABLED_InstallLatestStableFullInstallerOverChromeMetaInstaller) {
  chrome_user_installer_->OverInstall();
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestDevFullInstallerOverChromeMetaInstallerTest) {
  chrome_user_installer_->OverInstall();
}

TEST_F(MiniInstallTest,
    InstallChromeUsingMultiInstallUser) {
  chrome_user_installer_->InstallChromeUsingMultiInstall();
}

TEST_F(MiniInstallTest,
    InstallChromeUsingMultiInstallSys) {
  chrome_sys_installer_->InstallChromeUsingMultiInstall();
}

TEST_F(MiniInstallTest, InstallChromeAndChromeFrameUser) {
  chrome_user_installer_->InstallChromeAndChromeFrame(false);
}

TEST_F(MiniInstallTest, InstallChromeAndChromeFrameSys) {
  chrome_sys_installer_->InstallChromeAndChromeFrame(false);
}

TEST_F(MiniInstallTest,
    InstallChromeAndChromeFrameReadyModeUser) {
  chrome_user_installer_->InstallChromeAndChromeFrame(true);
}

TEST_F(MiniInstallTest,
    InstallChromeAndChromeFrameReadyModeSys) {
  chrome_sys_installer_->InstallChromeAndChromeFrame(true);
}

TEST_F(MiniInstallTest, InstallChromeFrameUsingMultiInstallUser) {
  cf_user_installer_->InstallChromeFrameUsingMultiInstall();
}

TEST_F(MiniInstallTest, InstallChromeFrameUsingMultiInstallSys) {
  cf_sys_installer_->InstallChromeFrameUsingMultiInstall();
}

// Chrome Frame is in use while Chrome is install.
TEST_F(MiniInstallTest, InstallChromeWithExistingChromeFrameMultiInstallUser) {
  cf_user_installer_->InstallChromeFrameUsingMultiInstall();
  chrome_user_installer_->InstallChromeUsingMultiInstall();
}

// Chrome Frame is in use while Chrome is install.
TEST_F(MiniInstallTest, InstallChromeWithExistingChromeFrameMultiInstallSys) {
  cf_sys_installer_->InstallChromeFrameUsingMultiInstall();
  chrome_sys_installer_->InstallChromeUsingMultiInstall();
}

TEST_F(MiniInstallTest, OverInstallChromeWhenInUseUser) {
  chrome_user_installer_->InstallChromeUsingMultiInstall();
  chrome_user_installer_->LaunchChrome(false);
  chrome_user_installer_->InstallChromeUsingMultiInstall();
}

TEST_F(MiniInstallTest, OverInstallChromeWhenInUseSys) {
  chrome_sys_installer_->InstallChromeUsingMultiInstall();
  chrome_sys_installer_->LaunchChrome(false);
  chrome_sys_installer_->InstallChromeUsingMultiInstall();
}

#endif

TEST_F(MiniInstallTest, InstallMiniInstallerSys) {
  chrome_sys_installer_->Install();
}

#if defined(OS_WIN)
// http://crbug.com/57157 - Fails on windows.
#define MAYBE_InstallMiniInstallerUser FLAKY_InstallMiniInstallerUser
#else
#define MAYBE_InstallMiniInstallerUser InstallMiniInstallerUser
#endif
TEST_F(MiniInstallTest, MAYBE_InstallMiniInstallerUser) {
  chrome_user_installer_->Install();
}

TEST(GenericInstallTest, MiniInstallTestValidWindowsVersion) {
  // We run the tests on all supported OSes.
  // Make sure the code agrees.
  EXPECT_TRUE(InstallUtil::IsOSSupported());
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
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
  MiniInstallTest() : chrome_frame_(false) {}

  static void CleanTheSystem() {
    const CommandLine* cmd = CommandLine::ForCurrentProcess();
    if (cmd->HasSwitch(installer::switches::kChromeFrame)) {
      ChromeMiniInstaller systeminstall(kSystemInstall,
          cmd->HasSwitch(installer::switches::kChromeFrame));
      systeminstall.UnInstall();
    } else {
      ChromeMiniInstaller userinstall(kUserInstall,
          cmd->HasSwitch(installer::switches::kChromeFrame));
      userinstall.UnInstall();
      ChromeMiniInstaller systeminstall(kSystemInstall,
          cmd->HasSwitch(installer::switches::kChromeFrame));
      systeminstall.UnInstall();
    }
  }

  virtual void SetUp() {
    // Parse test command-line arguments.
    const CommandLine* cmd = CommandLine::ForCurrentProcess();
    std::wstring build =
        cmd->GetSwitchValueNative(switches::kInstallerTestBuild);
    if (build.empty())
      build = L"latest";
    chrome_frame_ = cmd->HasSwitch(installer::switches::kChromeFrame);

    CleanTheSystem();
    // Separate the test output from cleaning output
    printf("\nBEGIN test----------------------------------------\n");

    // Create a few differently configured installers that are used in
    // the tests, for convenience.
    user_inst_.reset(new ChromeMiniInstaller(kUserInstall,
                                             chrome_frame_));
    sys_inst_.reset(new ChromeMiniInstaller(kSystemInstall,
                                            chrome_frame_));
    sys_inst_->SetBuildUnderTest(build);
    user_inst_->SetBuildUnderTest(build);
  }

  static void TearDownTestCase() {
    // Uninstall Chrome from the system after tests are run.
    CleanTheSystem();
  }

 protected:
  // Decided if ChromeFrame tests should be run.
  bool chrome_frame_;

  // Installers created in test fixture setup for convenience.
  scoped_ptr<ChromeMiniInstaller> user_inst_;
  scoped_ptr<ChromeMiniInstaller> sys_inst_;
};

}  // namespace

#if defined(GOOGLE_CHROME_BUILD)
// Could use a parameterized gtest to slim down this list of tests, but since
// these tests will often be run manually, don't want to have obscure test
// names.

// Install full installer at system level.
TEST_F(MiniInstallTest, FullInstallerSys) {
  sys_inst_->InstallFullInstaller(false);
}

// Install full installer at user level.
TEST_F(MiniInstallTest, FullInstallerUser) {
  if (!chrome_frame_)
    user_inst_->InstallFullInstaller(false);
}
// Overinstall full installer.
TEST_F(MiniInstallTest, FullOverPreviousFullUser) {
  if (!chrome_frame_)
    user_inst_->OverInstallOnFullInstaller(kFullInstall);
}
TEST_F(MiniInstallTest, FullOverPreviousFullSys) {
  sys_inst_->OverInstallOnFullInstaller(kFullInstall);
}

// Overinstall diff installer.
TEST_F(MiniInstallTest, DiffOverPreviousFullUser) {
  if (!chrome_frame_)
    user_inst_->OverInstallOnFullInstaller(kDiffInstall);
}

TEST_F(MiniInstallTest, DiffOverPreviousFullSys) {
  sys_inst_->OverInstallOnFullInstaller(kDiffInstall);
}

// Repair version folder.
TEST_F(MiniInstallTest, RepairFolderOnFullUser) {
  if (!chrome_frame_)
    user_inst_->Repair(ChromeMiniInstaller::VERSION_FOLDER);
}

TEST_F(MiniInstallTest, RepairFolderOnFullSys) {
  sys_inst_->Repair(ChromeMiniInstaller::VERSION_FOLDER);
}

// Repair registry.
TEST_F(MiniInstallTest, RepairRegistryOnFullUser) {
  if (!chrome_frame_)
    user_inst_->Repair(ChromeMiniInstaller::REGISTRY);
}
TEST_F(MiniInstallTest, RepairRegistryOnFullSys) {
  sys_inst_->Repair(ChromeMiniInstaller::REGISTRY);
}

// Install standalone.
TEST_F(MiniInstallTest, InstallStandaloneUser) {
  if (!chrome_frame_)
    user_inst_->InstallStandaloneInstaller();
}

// This test doesn't make sense. Disabling for now.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerOverChromeMetaInstallerTest) {
  if (!chrome_frame_)
    user_inst_->OverInstall();
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestStableFullInstallerOverChromeMetaInstaller) {
  if (!chrome_frame_)
    user_inst_->OverInstall();
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestDevFullInstallerOverChromeMetaInstallerTest) {
  if (!chrome_frame_)
    user_inst_->OverInstall();
}
#endif

TEST_F(MiniInstallTest, InstallMiniInstallerSys) {
  sys_inst_->Install();
}

#if defined(OS_WIN)
// http://crbug.com/57157 - Fails on windows.
#define MAYBE_InstallMiniInstallerUser FLAKY_InstallMiniInstallerUser
#else
#define MAYBE_InstallMiniInstallerUser InstallMiniInstallerUser
#endif
TEST_F(MiniInstallTest, MAYBE_InstallMiniInstallerUser) {
  user_inst_->Install();
}

TEST_F(MiniInstallTest, MiniInstallTestValidWindowsVersion) {
  // We run the tests on all supported OSes.
  // Make sure the code agrees.
  EXPECT_TRUE(InstallUtil::IsOSSupported());
}

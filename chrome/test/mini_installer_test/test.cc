// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/platform_thread.h"
#include "base/scoped_ptr.h"
#include "base/win_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome_mini_installer.h"

// Although the C++ style guide disallows use of namespace directive, use
// here because this is not only a .cc file, but also a test.
using namespace mini_installer_constants;

namespace {

class MiniInstallTest : public testing::Test {
   protected:
    // Whether these tests should be run regardless of our running platform.
    bool force_tests_;

    // Installers created in test fixture setup for convenience.
    scoped_ptr<ChromeMiniInstaller> user_inst_, sys_inst_;

   public:
    static void CleanTheSystem() {
      ChromeMiniInstaller userinstall(kUserInstall);
      userinstall.UnInstall();
      ChromeMiniInstaller systeminstall(kSystemInstall);
      systeminstall.UnInstall();
    }

    virtual void SetUp() {
      // Parse test command-line arguments.
      const CommandLine* cmd = CommandLine::ForCurrentProcess();
      std::wstring build = cmd->GetSwitchValue(L"build");
      if (build.empty())
        build = L"latest";

      force_tests_ = cmd->HasSwitch(L"force");

      if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA ||
          force_tests_) {
        CleanTheSystem();
        // Separate the test output from cleaning output
        printf("\nBEGIN test----------------------------------------\n");

        // Create a few differently configured installers that are used in
        // the tests, for convenience.
        user_inst_.reset(new ChromeMiniInstaller(kUserInstall));
        user_inst_->SetBuildUnderTest(build);
        sys_inst_.reset(new ChromeMiniInstaller(kSystemInstall));
        sys_inst_->SetBuildUnderTest(build);

      } else {
        printf("These tests don't run on this platform.\n");
        exit(0);
      }
    }

    static void TearDownTestCase() {
      // Uninstall Chrome from the system after tests are run.
      CleanTheSystem();
    }
};
};

#if defined(GOOGLE_CHROME_BUILD)
// Could use a parameterized gtest to slim down this list of tests, but since
// these tests will often be run manually, don't want to have obscure test
// names.

// Install full installer.
TEST_F(MiniInstallTest, FullInstallerUser) {
  user_inst_->InstallFullInstaller(false);
}
TEST_F(MiniInstallTest, FullInstallerSys) {
  sys_inst_->InstallFullInstaller(false);
}

// Overinstall full installer.
TEST_F(MiniInstallTest, FullOverPreviousFullUser) {
  user_inst_->OverInstallOnFullInstaller(kFullInstall);
}
TEST_F(MiniInstallTest, FullOverPreviousFullSys) {
  sys_inst_->OverInstallOnFullInstaller(kFullInstall);
}

// Overinstall diff installer.
TEST_F(MiniInstallTest, DiffOverPreviousFullUser) {
  user_inst_->OverInstallOnFullInstaller(kDiffInstall);
}
TEST_F(MiniInstallTest, DiffOverPreviousFullSys) {
  sys_inst_->OverInstallOnFullInstaller(kDiffInstall);
}

// Repair version folder.
TEST_F(MiniInstallTest, RepairFolderOnFullUser) {
  user_inst_->Repair(ChromeMiniInstaller::VERSION_FOLDER);
}
TEST_F(MiniInstallTest, RepairFolderOnFullSys) {
  sys_inst_->Repair(ChromeMiniInstaller::VERSION_FOLDER);
}

// Repair registry.
TEST_F(MiniInstallTest, RepairRegistryOnFullUser) {
  user_inst_->Repair(ChromeMiniInstaller::REGISTRY);
}
TEST_F(MiniInstallTest, RepairRegistryOnFullSys) {
  sys_inst_->Repair(ChromeMiniInstaller::REGISTRY);
}

// Install standalone.
TEST_F(MiniInstallTest, InstallStandaloneUser) {
  user_inst_->InstallStandaloneInstaller();
}

// This test doesn't make sense. Disabling for now.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerOverChromeMetaInstallerTest) {
  user_inst_->OverInstall();
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestStableFullInstallerOverChromeMetaInstaller) {
  user_inst_->OverInstall();
}
// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestDevFullInstallerOverChromeMetaInstallerTest) {
  user_inst_->OverInstall();
}
#endif

TEST_F(MiniInstallTest, InstallMiniInstallerSys) {
  sys_inst_->Install();
}

TEST_F(MiniInstallTest, InstallMiniInstallerUser) {
  user_inst_->Install();
}

TEST_F(MiniInstallTest, MiniInstallTestValidWindowsVersion) {
  // We run the tests on all supported OSes.
  // Make sure the code agrees.
  EXPECT_TRUE(InstallUtil::IsOSSupported());
}

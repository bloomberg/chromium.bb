// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/platform_thread.h"
#include "base/win_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome_mini_installer.h"

namespace {
class MiniInstallTest : public testing::Test {
   protected:
    void CleanTheSystem() {
      ChromeMiniInstaller userinstall(mini_installer_constants::kUserInstall);
      userinstall.UnInstall();
      ChromeMiniInstaller systeminstall(
            mini_installer_constants::kSystemInstall);
        systeminstall.UnInstall();
    }
    virtual void SetUp() {
      if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
        CleanTheSystem();
      } else {
        printf("These tests don't run on Vista\n");
        exit(0);
      }
    }
    virtual void TearDown() {
      if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
        PlatformThread::Sleep(2000);
        CleanTheSystem();
      } else {
        printf("These tests don't run on Vista\n");
        exit(0);
      }
    }
};
};

// TODO(nsylvain): Change this for GOOGLE_CHROME_BUILD when we have the
// previous installers accessible from our Google Chrome continuous buildbot.
#if defined(OFFICIAL_BUILD)
TEST_F(MiniInstallTest, InstallLatestDevFullInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.InstallFullInstaller(false,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest, InstallLatestDevFullInstallerTestSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.InstallFullInstaller(false,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest, InstallLatestStableFullInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.InstallFullInstaller(false,
      mini_installer_constants::kStableChannelBuild);
}

TEST_F(MiniInstallTest, InstallLatestStableFullInstallerTestSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.InstallFullInstaller(false,
      mini_installer_constants::kStableChannelBuild);
}

TEST_F(MiniInstallTest,
       InstallLatestDevFullInstallerOverPreviousFullDevInstaller) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kFullInstall,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest,
       InstallLatestDevFullInstallerOverPreviousFullDevInstallerSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kFullInstall,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest,
       InstallLatestDevDiffInstallerOverPreviousFullDevInstaller) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kDiffInstall,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest,
       InstallLatestDevDiffInstallerOverPreviousFullDevInstallerSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kDiffInstall,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest,
       InstallLatestFullStableInstallerOverPreviousFullStableInstaller) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kFullInstall,
      mini_installer_constants::kStableChannelBuild);
}

TEST_F(MiniInstallTest,
    InstallLatestFullStableInstallerOverPreviousFullStableInstallerSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kFullInstall,
      mini_installer_constants::kStableChannelBuild);
}

TEST_F(MiniInstallTest,
       InstallLatestDiffStableInstallerOverPreviousFullStableInstaller) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kDiffInstall,
      mini_installer_constants::kStableChannelBuild);
}

TEST_F(MiniInstallTest,
    InstallLatestDiffStableInstallerOverPreviousFullStableInstallerSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.OverInstallOnFullInstaller(mini_installer_constants::kDiffInstall,
      mini_installer_constants::kStableChannelBuild);
}

TEST_F(MiniInstallTest, StandaloneInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.InstallStandaloneInstaller();
}

// This test doesn't make sense. Disabling for now.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerOverChromeMetaInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstall(mini_installer_constants::kDevChannelBuild);
}

// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestStableFullInstallerOverChromeMetaInstaller) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstall(mini_installer_constants::kStableChannelBuild);
}
// Encountering issue 9593. Disabling temporarily.
TEST_F(MiniInstallTest,
       DISABLED_InstallLatestDevFullInstallerOverChromeMetaInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstall(mini_installer_constants::kDevChannelBuild);
}

// Repair testcases

TEST_F(MiniInstallTest, RepairFolderTestOnLatestDevFullInstaller) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.Repair(ChromeMiniInstaller::VERSION_FOLDER,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest, RepairFolderTestOnLatestDevFullInstallerSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.Repair(ChromeMiniInstaller::VERSION_FOLDER,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest, RepairRegistryTestOnLatestDevFullInstaller) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.Repair(ChromeMiniInstaller::REGISTRY,
      mini_installer_constants::kDevChannelBuild);
}

TEST_F(MiniInstallTest, RepairRegistryTestOnLatestDevFullInstallerSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.Repair(ChromeMiniInstaller::REGISTRY,
      mini_installer_constants::kDevChannelBuild);
}
#endif

TEST_F(MiniInstallTest, InstallLatestMiniInstallerAtSystemLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.Install();
}

TEST_F(MiniInstallTest, InstallLatestMiniInstallerAtUserLevel) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.Install();
}

TEST(InstallUtilTests, MiniInstallTestValidWindowsVersion) {
  // We run the tests on all supported OSes.
  // Make sure the code agrees.
  EXPECT_TRUE(InstallUtil::IsOSSupported());
}

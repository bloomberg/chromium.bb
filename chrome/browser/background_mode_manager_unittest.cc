// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;

class BackgroundModeManagerTest : public testing::Test {
 public:
  BackgroundModeManagerTest() {}
  ~BackgroundModeManagerTest() {}
  void SetUp() {
    command_line_.reset(new CommandLine(CommandLine::ARGUMENTS_ONLY));
    command_line_->AppendSwitch(switches::kEnableBackgroundMode);
  }
  scoped_ptr<CommandLine> command_line_;
};

class TestBackgroundModeManager : public BackgroundModeManager {
 public:
  TestBackgroundModeManager(Profile* profile, CommandLine* cl)
      : BackgroundModeManager(profile, cl) {
  }
  MOCK_METHOD1(EnableLaunchOnStartup, void(bool));
  MOCK_METHOD0(CreateStatusTrayIcon, void());
  MOCK_METHOD0(RemoveStatusTrayIcon, void());
};

TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile, command_line_.get());
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  EXPECT_FALSE(BrowserList::WillKeepAlive());
  // Call to AppLoaded() will cause the status tray to be created, then call to
  // unloaded will result in call to remove the icon.
  manager.OnBackgroundAppLoaded();
  EXPECT_TRUE(BrowserList::WillKeepAlive());
  manager.OnBackgroundAppUnloaded();
  EXPECT_FALSE(BrowserList::WillKeepAlive());
}

TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstall) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile, command_line_.get());
  // Call to AppInstalled() will cause chrome to be set to launch on startup,
  // and call to AppUninstalled() set chrome to not launch on startup.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled();
}

TEST_F(BackgroundModeManagerTest, BackgroundPrefDisabled) {
  InSequence s;
  TestingProfile profile;
  profile.GetPrefs()->SetBoolean(prefs::kBackgroundModeEnabled, false);
  TestBackgroundModeManager manager(&profile, command_line_.get());
  EXPECT_CALL(manager, CreateStatusTrayIcon()).Times(0);
  // Should not change launch on startup status when installing/uninstalling
  // if background mode is disabled.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(0);
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  EXPECT_FALSE(BrowserList::WillKeepAlive());
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled();
}

TEST_F(BackgroundModeManagerTest, BackgroundPrefDynamicDisable) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile, command_line_.get());
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  EXPECT_TRUE(BrowserList::WillKeepAlive());
  // Disable status on the fly.
  profile.GetPrefs()->SetBoolean(prefs::kBackgroundModeEnabled, false);
  // Manually notify background mode manager that pref has changed
  manager.OnBackgroundModePrefChanged();
  EXPECT_FALSE(BrowserList::WillKeepAlive());
}

TEST_F(BackgroundModeManagerTest, BackgroundPrefDynamicEnable) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile, command_line_.get());
  profile.GetPrefs()->SetBoolean(prefs::kBackgroundModeEnabled, false);
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  EXPECT_FALSE(BrowserList::WillKeepAlive());
  // Enable status on the fly.
  profile.GetPrefs()->SetBoolean(prefs::kBackgroundModeEnabled, true);
  EXPECT_TRUE(BrowserList::WillKeepAlive());
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;

class TestBackgroundModeManager : public BackgroundModeManager {
 public:
  explicit TestBackgroundModeManager(Profile* profile)
      : BackgroundModeManager(profile) {
  }
  MOCK_METHOD1(EnableLaunchOnStartup, void(bool));
  MOCK_METHOD0(CreateStatusTrayIcon, void());
  MOCK_METHOD0(RemoveStatusTrayIcon, void());
};

TEST(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile);
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

TEST(BackgroundModeManagerTest, BackgroundAppInstallUninstall) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile);
  // Call to AppInstalled() will cause chrome to be set to launch on startup,
  // and call to AppUninstalling() set chrome to not launch on startup.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  manager.OnBackgroundAppUninstalled();
  manager.OnBackgroundAppUnloaded();
}

TEST(BackgroundModeManagerTest, BackgroundPrefDisabled) {
  InSequence s;
  TestingProfile profile;
  profile.GetPrefs()->SetBoolean(prefs::kBackgroundModeEnabled, false);
  TestBackgroundModeManager manager(&profile);
  // Should not change launch on startup status when installing/uninstalling
  // if background mode is disabled.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(0);
  EXPECT_CALL(manager, CreateStatusTrayIcon()).Times(0);
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  EXPECT_FALSE(BrowserList::WillKeepAlive());
  manager.OnBackgroundAppUninstalled();
  manager.OnBackgroundAppUnloaded();
}

TEST(BackgroundModeManagerTest, BackgroundPrefDynamicDisable) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile);
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  EXPECT_TRUE(BrowserList::WillKeepAlive());
  // Disable status on the fly.
  profile.GetPrefs()->SetBoolean(prefs::kBackgroundModeEnabled, false);
  EXPECT_FALSE(BrowserList::WillKeepAlive());
}

TEST(BackgroundModeManagerTest, BackgroundPrefDynamicEnable) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile);
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

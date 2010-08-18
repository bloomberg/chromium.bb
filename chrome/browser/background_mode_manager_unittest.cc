// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile);
  EXPECT_FALSE(BrowserList::WillKeepAlive());
  // Call to AppLoaded() will cause the status tray to be created, then call to
  // unloaded will result in call to remove the icon.
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  manager.OnBackgroundAppLoaded();
  EXPECT_TRUE(BrowserList::WillKeepAlive());
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  manager.OnBackgroundAppUnloaded();
  EXPECT_FALSE(BrowserList::WillKeepAlive());
}

TEST(BackgroundModeManagerTest, BackgroundAppInstallUninstall) {
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile);
  // Call to AppInstalled() will cause chrome to be set to launch on startup,
  // and call to AppUninstalling() set chrome to not launch on startup.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  manager.OnBackgroundAppInstalled();
  manager.OnBackgroundAppLoaded();
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  manager.OnBackgroundAppUninstalled();
  manager.OnBackgroundAppUnloaded();
}

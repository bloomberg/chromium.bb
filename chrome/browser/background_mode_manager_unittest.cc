// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::InSequence;
using testing::Return;

class BackgroundModeManagerTest : public TestingBrowserProcessTest {
 public:
  BackgroundModeManagerTest() {}
  ~BackgroundModeManagerTest() {}
  void SetUp() {
    command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
  }
  scoped_ptr<CommandLine> command_line_;
};

class TestBackgroundModeManager : public BackgroundModeManager {
 public:
  TestBackgroundModeManager(Profile* profile, CommandLine* cl)
      : BackgroundModeManager(profile, cl),
        enabled_(true) {}
  MOCK_METHOD1(EnableLaunchOnStartup, void(bool));
  MOCK_METHOD0(CreateStatusTrayIcon, void());
  MOCK_METHOD0(RemoveStatusTrayIcon, void());
  virtual bool IsBackgroundModePrefEnabled() { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }
 private:
  bool enabled_;
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
  manager.OnBackgroundAppInstalled(NULL);
  manager.OnBackgroundAppLoaded();
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled();
}

// App installs while disabled should do nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile, command_line_.get());
  // Turn off background mode.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();

  // Status tray icons will not be created, launch on startup status will be set
  // to "do not launch on startup".
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  manager.OnBackgroundAppInstalled(NULL);
  manager.OnBackgroundAppLoaded();
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled();

  // Re-enable background mode.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
}


// App installs while disabled should do nothing.
TEST_F(BackgroundModeManagerTest, EnableAfterBackgroundAppInstall) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(&profile, command_line_.get());
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  EXPECT_CALL(manager, CreateStatusTrayIcon());
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, RemoveStatusTrayIcon());
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.OnBackgroundAppLoaded();

  // Turn off background mode - should hide status tray icon.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();

  // Turn back on background mode - should show status tray icon.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();

  // Uninstall app, should hide status tray icon again.
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled();
}

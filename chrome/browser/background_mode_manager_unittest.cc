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

using testing::_;
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
  explicit TestBackgroundModeManager(CommandLine* command_line)
      : BackgroundModeManager(command_line),
        enabled_(true) {}
  MOCK_METHOD1(EnableLaunchOnStartup, void(bool));
  MOCK_METHOD1(CreateStatusTrayIcon, void(Profile*));  // NOLINT
  MOCK_METHOD1(RemoveStatusTrayIcon, void(Profile*));  // NOLINT
  virtual bool IsBackgroundModePrefEnabled() { return enabled_; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }
 private:
  bool enabled_;
};

TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile);
  EXPECT_CALL(manager, RemoveStatusTrayIcon(_));
  EXPECT_FALSE(BrowserList::WillKeepAlive());
  // Call to AppLoaded() will not cause the status tray to be created,
  // because no apps have been installed. However the call to AppUnloaded()
  // will result in a call RemoveStatusTrayIcon since it will try to unload
  // all icons now that there are no apps.
  manager.OnBackgroundAppLoaded();
  EXPECT_TRUE(BrowserList::WillKeepAlive());
  manager.OnBackgroundAppUnloaded();
  EXPECT_FALSE(BrowserList::WillKeepAlive());
}

TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstall) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile);
  // Call to AppInstalled() will cause chrome to be set to launch on startup,
  // and call to AppUninstalled() set chrome to not launch on startup.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon(_));
  EXPECT_CALL(manager, RemoveStatusTrayIcon(_)).Times(2);
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  manager.OnBackgroundAppInstalled(NULL, &profile);
  manager.OnBackgroundAppLoaded();
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled(&profile);}

// App installs while disabled should do nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile);
  // Turn off background mode.
  EXPECT_CALL(manager, RemoveStatusTrayIcon(_));
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();

  // Status tray icons will not be created, launch on startup status will be set
  // to "do not launch on startup".
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  manager.OnBackgroundAppInstalled(NULL, &profile);
  manager.OnBackgroundAppLoaded();
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled(&profile);

  // Re-enable background mode.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
}


// App installs while disabled should do nothing.
TEST_F(BackgroundModeManagerTest, EnableAfterBackgroundAppInstall) {
  InSequence s;
  TestingProfile profile;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile);
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon(_));
  EXPECT_CALL(manager, RemoveStatusTrayIcon(_));
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, RemoveStatusTrayIcon(_)).Times(2);
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL, &profile);
  // OnBackgroundAppInstalled does not actually add an app to the
  // BackgroundApplicationListModel which would result in another
  // call to CreateStatusTray.
  manager.OnBackgroundAppLoaded();

  // Turn off background mode - should hide status tray icon.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();

  // Turn back on background mode - again, no status tray icon
  // will show up since we didn't actually add anything to the list.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();

  // Uninstall app, should hide status tray icon again.
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled(&profile);
}

TEST_F(BackgroundModeManagerTest, MultiProfile) {
  InSequence s;
  TestingProfile profile1;
  TestingProfile profile2;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile1);
  manager.RegisterProfile(&profile2);
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, CreateStatusTrayIcon(_)).Times(2);
  EXPECT_CALL(manager, RemoveStatusTrayIcon(_)).Times(2);
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  EXPECT_CALL(manager, EnableLaunchOnStartup(true));
  EXPECT_CALL(manager, RemoveStatusTrayIcon(_)).Times(4);
  EXPECT_CALL(manager, EnableLaunchOnStartup(false));
  EXPECT_FALSE(BrowserList::WillKeepAlive());

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL, &profile1);
  // OnBackgroundAppInstalled does not actually add an app to the
  // BackgroundApplicationListModel which would result in another
  // call to CreateStatusTray.
  manager.OnBackgroundAppLoaded();

  // Install app for other profile, hsould show other status tray icon.
  manager.OnBackgroundAppInstalled(NULL, &profile2);
  manager.OnBackgroundAppLoaded();

  // Should hide both status tray icons.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();

  // Turn back on background mode - should show both status tray icons.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();

  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled(&profile1);
  // There is still one background app alive
  EXPECT_TRUE(BrowserList::WillKeepAlive());
  manager.OnBackgroundAppUnloaded();
  manager.OnBackgroundAppUninstalled(&profile2);
  EXPECT_FALSE(BrowserList::WillKeepAlive());
}

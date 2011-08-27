// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class BackgroundModeManagerTest : public testing::Test {
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
        enabled_(true),
        app_count_(0),
        have_status_tray_(false),
        launch_on_startup_(false) {}
  virtual void EnableLaunchOnStartup(bool launch) OVERRIDE {
    launch_on_startup_ = launch;
  }
  virtual void CreateStatusTrayIcon() OVERRIDE { have_status_tray_ = true; }
  virtual void RemoveStatusTrayIcon() OVERRIDE { have_status_tray_ = false; }
  virtual int GetBackgroundAppCount() const OVERRIDE { return app_count_; }
  virtual bool IsBackgroundModePrefEnabled() const OVERRIDE { return enabled_; }
  void SetBackgroundAppCount(int count) { app_count_ = count; }
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  bool HaveStatusTray() const { return have_status_tray_; }
  bool IsLaunchOnStartup() const { return launch_on_startup_; }
 private:
  bool enabled_;
  int app_count_;

  // Flags to track whether we are launching on startup/have a status tray.
  bool have_status_tray_;
  bool launch_on_startup_;
};

static void AssertBackgroundModeActive(
    const TestBackgroundModeManager& manager) {
  EXPECT_TRUE(BrowserList::WillKeepAlive());
  EXPECT_TRUE(manager.HaveStatusTray());
  EXPECT_TRUE(manager.IsLaunchOnStartup());
}

static void AssertBackgroundModeInactive(
    const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(BrowserList::WillKeepAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
  EXPECT_FALSE(manager.IsLaunchOnStartup());
}

TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  TestingProfile profile;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile);
  EXPECT_FALSE(BrowserList::WillKeepAlive());

  // Mimic app load.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(&profile);
  AssertBackgroundModeActive(manager);

  // Mimic app unload.
  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(&profile);
  AssertBackgroundModeInactive(manager);
}

// App installs while background mode is disabled should do nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  TestingProfile profile;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile);
  // Turn off background mode.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Status tray icons will not be created, launch on startup status will not
  // be modified.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(&profile);
  AssertBackgroundModeInactive(manager);

  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(&profile);
  AssertBackgroundModeInactive(manager);

  // Re-enable background mode.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  AssertBackgroundModeInactive(manager);
}


// App installs while disabled should do nothing until background mode is
// enabled..
TEST_F(BackgroundModeManagerTest, EnableAfterBackgroundAppInstall) {
  TestingProfile profile;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile);

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  // OnBackgroundAppInstalled does not actually add an app to the
  // BackgroundApplicationListModel which would result in another
  // call to CreateStatusTray.
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(&profile);
  AssertBackgroundModeActive(manager);

  // Turn off background mode - should hide status tray icon.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Turn back on background mode - again, no status tray icon
  // will show up since we didn't actually add anything to the list.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  AssertBackgroundModeActive(manager);

  // Uninstall app, should hide status tray icon again.
  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(&profile);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, MultiProfile) {
  TestingProfile profile1;
  TestingProfile profile2;
  TestBackgroundModeManager manager(command_line_.get());
  manager.RegisterProfile(&profile1);
  manager.RegisterProfile(&profile2);
  EXPECT_FALSE(BrowserList::WillKeepAlive());

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(&profile1);
  AssertBackgroundModeActive(manager);

  // Install app for other profile, hsould show other status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(2);
  manager.OnApplicationListChanged(&profile2);
  AssertBackgroundModeActive(manager);

  // Should hide both status tray icons.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Turn back on background mode - should show both status tray icons.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  AssertBackgroundModeActive(manager);

  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(&profile2);
  // There is still one background app alive
  AssertBackgroundModeActive(manager);

  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(&profile1);
  AssertBackgroundModeInactive(manager);
}

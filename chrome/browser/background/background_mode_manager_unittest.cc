// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

class BackgroundModeManagerTest : public testing::Test {
 public:
  BackgroundModeManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~BackgroundModeManagerTest() {}
  void SetUp() {
    command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
    ASSERT_TRUE(profile_manager_.SetUp());
  }
  scoped_ptr<CommandLine> command_line_;
  TestingProfileManager profile_manager_;
};

class TestBackgroundModeManager : public BackgroundModeManager {
 public:
  TestBackgroundModeManager(
      CommandLine* command_line, ProfileInfoCache* cache, bool enabled)
      : BackgroundModeManager(command_line, cache),
        enabled_(enabled),
        app_count_(0),
        profile_app_count_(0),
        have_status_tray_(false),
        launch_on_startup_(false) {}
  virtual void EnableLaunchOnStartup(bool launch) OVERRIDE {
    launch_on_startup_ = launch;
  }
  virtual void CreateStatusTrayIcon() OVERRIDE { have_status_tray_ = true; }
  virtual void RemoveStatusTrayIcon() OVERRIDE { have_status_tray_ = false; }
  virtual int GetBackgroundAppCount() const OVERRIDE { return app_count_; }
  virtual int GetBackgroundAppCountForProfile(
      Profile* const profile) const OVERRIDE {
    return profile_app_count_;
  }
  virtual bool IsBackgroundModePrefEnabled() const OVERRIDE { return enabled_; }
  void SetBackgroundAppCount(int count) { app_count_ = count; }
  void SetBackgroundAppCountForProfile(int count) {
    profile_app_count_ = count;
  }
  void SetEnabled(bool enabled) {
    enabled_ = enabled;
    OnBackgroundModeEnabledPrefChanged();
  }
  bool HaveStatusTray() const { return have_status_tray_; }
  bool IsLaunchOnStartup() const { return launch_on_startup_; }
 private:
  bool enabled_;
  int app_count_;
  int profile_app_count_;

  // Flags to track whether we are launching on startup/have a status tray.
  bool have_status_tray_;
  bool launch_on_startup_;
};

static void AssertBackgroundModeActive(
    const TestBackgroundModeManager& manager) {
  EXPECT_TRUE(browser::WillKeepAlive());
  EXPECT_TRUE(manager.HaveStatusTray());
  EXPECT_TRUE(manager.IsLaunchOnStartup());
}

static void AssertBackgroundModeInactive(
    const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(browser::WillKeepAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
  EXPECT_FALSE(manager.IsLaunchOnStartup());
}

TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  TestingProfile* profile = profile_manager_.CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), true);
  manager.RegisterProfile(profile);
  EXPECT_FALSE(browser::WillKeepAlive());

  // Mimic app load.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeActive(manager);

  // Mimic app unload.
  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeInactive(manager);
}

// App installs while background mode is disabled should do nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  TestingProfile* profile = profile_manager_.CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), true);
  manager.RegisterProfile(profile);
  // Turn off background mode.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Status tray icons will not be created, launch on startup status will not
  // be modified.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeInactive(manager);

  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeInactive(manager);

  // Re-enable background mode.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  AssertBackgroundModeInactive(manager);
}


// App installs while disabled should do nothing until background mode is
// enabled..
TEST_F(BackgroundModeManagerTest, EnableAfterBackgroundAppInstall) {
  TestingProfile* profile = profile_manager_.CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), true);
  manager.RegisterProfile(profile);

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  // OnBackgroundAppInstalled does not actually add an app to the
  // BackgroundApplicationListModel which would result in another
  // call to CreateStatusTray.
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile);
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
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, MultiProfile) {
  TestingProfile* profile1 = profile_manager_.CreateTestingProfile("p1");
  TestingProfile* profile2 = profile_manager_.CreateTestingProfile("p2");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(browser::WillKeepAlive());

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile1);
  AssertBackgroundModeActive(manager);

  // Install app for other profile, hsould show other status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(2);
  manager.OnApplicationListChanged(profile2);
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
  manager.OnApplicationListChanged(profile2);
  // There is still one background app alive
  AssertBackgroundModeActive(manager);

  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(profile1);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, ProfileInfoCacheStorage) {
  TestingProfile* profile1 = profile_manager_.CreateTestingProfile("p1");
  TestingProfile* profile2 = profile_manager_.CreateTestingProfile("p2");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(browser::WillKeepAlive());

  ProfileInfoCache* cache = profile_manager_.profile_info_cache();
  EXPECT_EQ(2u, cache->GetNumberOfProfiles());

  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(1));

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.SetBackgroundAppCountForProfile(1);
  manager.OnApplicationListChanged(profile1);

  // Install app for other profile.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.SetBackgroundAppCountForProfile(1);
  manager.OnApplicationListChanged(profile2);

  EXPECT_TRUE(cache->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_TRUE(cache->GetBackgroundStatusOfProfileAtIndex(1));

  manager.SetBackgroundAppCountForProfile(0);
  manager.OnApplicationListChanged(profile1);

  size_t p1_index = cache->GetIndexOfProfileWithPath(profile1->GetPath());
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(p1_index));

  manager.SetBackgroundAppCountForProfile(0);
  manager.OnApplicationListChanged(profile2);

  size_t p2_index = cache->GetIndexOfProfileWithPath(profile1->GetPath());
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(p2_index));

  // Even though neither has background status on, there should still be two
  // profiles in the cache.
  EXPECT_EQ(2u, cache->GetNumberOfProfiles());
}

TEST_F(BackgroundModeManagerTest, ProfileInfoCacheObserver) {
  TestingProfile* profile1 = profile_manager_.CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  EXPECT_FALSE(browser::WillKeepAlive());

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.SetBackgroundAppCountForProfile(1);
  manager.OnApplicationListChanged(profile1);

  manager.OnProfileNameChanged(
      profile1->GetPath(),
      manager.GetBackgroundModeData(profile1)->name());

  EXPECT_EQ(UTF8ToUTF16("p1"),
            manager.GetBackgroundModeData(profile1)->name());

  TestingProfile* profile2 = profile_manager_.CreateTestingProfile("p2");
  manager.RegisterProfile(profile2);
  EXPECT_EQ(2, manager.NumberOfBackgroundModeData());

  manager.OnProfileAdded(profile2->GetPath());
  EXPECT_EQ(UTF8ToUTF16("p2"),
            manager.GetBackgroundModeData(profile2)->name());

  manager.OnProfileWillBeRemoved(profile2->GetPath());
  EXPECT_EQ(1, manager.NumberOfBackgroundModeData());

  // Check that the background mode data we think is in the map actually is.
  EXPECT_EQ(UTF8ToUTF16("p1"),
            manager.GetBackgroundModeData(profile1)->name());
}

TEST_F(BackgroundModeManagerTest, DisableBackgroundModeUnderTestFlag) {
  TestingProfile* profile1 = profile_manager_.CreateTestingProfile("p1");
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  EXPECT_TRUE(manager.ShouldBeInBackgroundMode());
  manager.SetEnabled(false);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerTest,
       BackgroundModeDisabledPreventsKeepAliveOnStartup) {
  TestingProfile* profile1 = profile_manager_.CreateTestingProfile("p1");
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_.profile_info_cache(), false);
  manager.RegisterProfile(profile1);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

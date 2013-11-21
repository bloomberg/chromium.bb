// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

class BackgroundModeManagerTest : public testing::Test {
 public:
  BackgroundModeManagerTest() {}
  virtual ~BackgroundModeManagerTest() {}
  virtual void SetUp() {
    command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
  }
  scoped_ptr<CommandLine> command_line_;

 protected:
  scoped_refptr<extensions::Extension> CreateExtension(
      extensions::Manifest::Location location,
      const std::string& data,
      const std::string& id) {
    scoped_ptr<base::DictionaryValue> parsed_manifest(
        extension_function_test_utils::ParseDictionary(data));
    return extension_function_test_utils::CreateExtension(
        location,
        parsed_manifest.get(),
        id);
  }

  scoped_ptr<TestingProfileManager> CreateTestingProfileManager() {
    scoped_ptr<TestingProfileManager> profile_manager
        (new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    EXPECT_TRUE(profile_manager->SetUp());
    return profile_manager.Pass();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManagerTest);
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
        launch_on_startup_(false) {
    ResumeBackgroundMode();
  }
  virtual void EnableLaunchOnStartup(bool launch) OVERRIDE {
    launch_on_startup_ = launch;
  }
  virtual void DisplayAppInstalledNotification(
      const extensions::Extension* extension) OVERRIDE {}
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
  EXPECT_TRUE(chrome::WillKeepAlive());
  EXPECT_TRUE(manager.HaveStatusTray());
  EXPECT_TRUE(manager.IsLaunchOnStartup());
}

static void AssertBackgroundModeInactive(
    const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(chrome::WillKeepAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
  EXPECT_FALSE(manager.IsLaunchOnStartup());
}

static void AssertBackgroundModeSuspended(
    const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(chrome::WillKeepAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
  EXPECT_TRUE(manager.IsLaunchOnStartup());
}

TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile = profile_manager->CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), true);
  manager.RegisterProfile(profile);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Mimic app load.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeActive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeSuspended(manager);
  manager.ResumeBackgroundMode();

  // Mimic app unload.
  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeInactive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Mimic app load while suspended, e.g. from sync. This should enable and
  // resume background mode.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile);
  AssertBackgroundModeActive(manager);
}

// App installs while background mode is disabled should do nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile = profile_manager->CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), true);
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
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile = profile_manager->CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), true);
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
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile1 = profile_manager->CreateTestingProfile("p1");
  TestingProfile* profile2 = profile_manager->CreateTestingProfile("p2");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(chrome::WillKeepAlive());

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
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile1 = profile_manager->CreateTestingProfile("p1");
  TestingProfile* profile2 = profile_manager->CreateTestingProfile("p2");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(chrome::WillKeepAlive());

  ProfileInfoCache* cache = profile_manager->profile_info_cache();
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
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile1 = profile_manager->CreateTestingProfile("p1");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  EXPECT_FALSE(chrome::WillKeepAlive());

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

  TestingProfile* profile2 = profile_manager->CreateTestingProfile("p2");
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
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile1 = profile_manager->CreateTestingProfile("p1");
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), true);
  manager.RegisterProfile(profile1);
  EXPECT_TRUE(manager.ShouldBeInBackgroundMode());
  manager.SetEnabled(false);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerTest,
       BackgroundModeDisabledPreventsKeepAliveOnStartup) {
  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();
  TestingProfile* profile1 = profile_manager->CreateTestingProfile("p1");
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager->profile_info_cache(), false);
  manager.RegisterProfile(profile1);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerTest, BackgroundMenuGeneration) {
  // Aura clears notifications from the message center at shutdown.
  message_center::MessageCenter::Initialize();

  // Required for extension service.
  content::TestBrowserThreadBundle thread_bundle;

  scoped_ptr<TestingProfileManager> profile_manager =
      CreateTestingProfileManager();

  // BackgroundModeManager actually affects Chrome start/stop state,
  // tearing down our thread bundle before we've had chance to clean
  // everything up. Keeping Chrome alive prevents this.
  // We aren't interested in if the keep alive works correctly in this test.
  chrome::StartKeepAlive();
  TestingProfile* profile = profile_manager->CreateTestingProfile("p");

#if defined(OS_CHROMEOS)
  // ChromeOS needs extra services to run in the following order.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service;
  chromeos::ScopedTestCrosSettings test_cros_settings;
  chromeos::ScopedTestUserManager test_user_manager;

  // On ChromeOS shutdown, HandleAppExitingForPlatform will call
  // chrome::EndKeepAlive because it assumes the aura shell
  // called chrome::StartKeepAlive. Simulate the call here.
  chrome::StartKeepAlive();
#endif

  scoped_refptr<extensions::Extension> component_extension(
    CreateExtension(
        extensions::Manifest::COMPONENT,
        "{\"name\": \"Component Extension\","
        "\"version\": \"1.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"background\"]}",
        "ID-1"));

  scoped_refptr<extensions::Extension> component_extension_with_options(
    CreateExtension(
        extensions::Manifest::COMPONENT,
        "{\"name\": \"Component Extension with Options\","
        "\"version\": \"1.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"background\"],"
        "\"options_page\": \"test.html\"}",
        "ID-2"));

  scoped_refptr<extensions::Extension> regular_extension(
    CreateExtension(
        extensions::Manifest::COMMAND_LINE,
        "{\"name\": \"Regular Extension\", "
        "\"version\": \"1.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"background\"]}",
        "ID-3"));

  scoped_refptr<extensions::Extension> regular_extension_with_options(
    CreateExtension(
        extensions::Manifest::COMMAND_LINE,
        "{\"name\": \"Regular Extension with Options\","
        "\"version\": \"1.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"background\"],"
        "\"options_page\": \"test.html\"}",
        "ID-4"));

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile))->CreateExtensionService(
          CommandLine::ForCurrentProcess(),
          base::FilePath(),
          false);
  ExtensionService* service = profile->GetExtensionService();
  service->Init();

  service->AddComponentExtension(component_extension);
  service->AddComponentExtension(component_extension_with_options);
  service->AddExtension(regular_extension);
  service->AddExtension(regular_extension_with_options);

  scoped_ptr<TestBackgroundModeManager> manager(new TestBackgroundModeManager(
      command_line_.get(), profile_manager->profile_info_cache(), true));
  manager->RegisterProfile(profile);

  scoped_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(NULL));
  scoped_ptr<StatusIconMenuModel> submenu(new StatusIconMenuModel(NULL));
  BackgroundModeManager::BackgroundModeData* bmd =
      manager->GetBackgroundModeData(profile);
  bmd->BuildProfileMenu(submenu.get(), menu.get());
  EXPECT_TRUE(
      submenu->GetLabelAt(0) ==
          UTF8ToUTF16("Component Extension"));
  EXPECT_FALSE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(0)));
  EXPECT_TRUE(
      submenu->GetLabelAt(1) ==
          UTF8ToUTF16("Component Extension with Options"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(1)));
  EXPECT_TRUE(
      submenu->GetLabelAt(2) ==
          UTF8ToUTF16("Regular Extension"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(2)));
  EXPECT_TRUE(
      submenu->GetLabelAt(3) ==
          UTF8ToUTF16("Regular Extension with Options"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(3)));

  // We have to destroy the profile now because we created it with real thread
  // state. This causes a lot of machinery to spin up that stops working
  // when we tear down our thread state at the end of the test.
  profile_manager->DeleteTestingProfile("p");

  // We're getting ready to shutdown the message loop. Clear everything out!
  base::MessageLoop::current()->RunUntilIdle();
  chrome::EndKeepAlive(); // Matching the above chrome::StartKeepAlive().

  // TestBackgroundModeManager has dependencies on the infrastructure.
  // It should get cleared first.
  manager.reset();

  // The Profile Manager references the Browser Process.
  // The Browser Process references the Notification UI Manager.
  // The Notification UI Manager references the Message Center.
  // As a result, we have to clear the browser process state here
  // before tearing down the Message Center.
  profile_manager.reset();

  // Message Center shutdown must occur after the EndKeepAlive because
  // EndKeepAlive will end up referencing the message center during cleanup.
  message_center::MessageCenter::Shutdown();
}

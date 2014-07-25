// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_shutdown.h"
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
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

// Helper class that tracks state transitions in BackgroundModeManager and
// exposes them via getters.
class SimpleTestBackgroundModeManager : public BackgroundModeManager {
 public:
  SimpleTestBackgroundModeManager(
      CommandLine* command_line, ProfileInfoCache* cache)
      : BackgroundModeManager(command_line, cache),
        have_status_tray_(false),
        launch_on_startup_(false),
        has_shown_balloon_(false) {
    ResumeBackgroundMode();
  }

  virtual void EnableLaunchOnStartup(bool launch) OVERRIDE {
    launch_on_startup_ = launch;
  }

  virtual void DisplayAppInstalledNotification(
      const extensions::Extension* extension) OVERRIDE {
    has_shown_balloon_ = true;
  }
  virtual void CreateStatusTrayIcon() OVERRIDE { have_status_tray_ = true; }
  virtual void RemoveStatusTrayIcon() OVERRIDE { have_status_tray_ = false; }

  bool HaveStatusTray() const { return have_status_tray_; }
  bool IsLaunchOnStartup() const { return launch_on_startup_; }
  bool HasShownBalloon() const { return has_shown_balloon_; }
  void SetHasShownBalloon(bool value) { has_shown_balloon_ = value; }

 private:
  // Flags to track whether we are launching on startup/have a status tray.
  bool have_status_tray_;
  bool launch_on_startup_;
  bool has_shown_balloon_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTestBackgroundModeManager);
};

class TestStatusIcon : public StatusIcon {
 public:
  TestStatusIcon() {}
  virtual void SetImage(const gfx::ImageSkia& image) OVERRIDE {}
  virtual void SetPressedImage(const gfx::ImageSkia& image) OVERRIDE {}
  virtual void SetToolTip(const base::string16& tool_tip) OVERRIDE {}
  virtual void DisplayBalloon(const gfx::ImageSkia& icon,
                              const base::string16& title,
                              const base::string16& contents) OVERRIDE {}
  virtual void UpdatePlatformContextMenu(
      StatusIconMenuModel* menu) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStatusIcon);
};

} // namespace

// More complex test helper that exposes APIs for fine grained control of
// things like the number of background applications. This allows writing
// smaller tests that don't have to install/uninstall extensions.
class TestBackgroundModeManager : public SimpleTestBackgroundModeManager {
 public:
  TestBackgroundModeManager(
      CommandLine* command_line, ProfileInfoCache* cache, bool enabled)
      : SimpleTestBackgroundModeManager(command_line, cache),
        enabled_(enabled),
        app_count_(0),
        profile_app_count_(0) {
    ResumeBackgroundMode();
  }

  virtual int GetBackgroundAppCount() const OVERRIDE { return app_count_; }
  virtual int GetBackgroundAppCountForProfile(
      Profile* const profile) const OVERRIDE {
    return profile_app_count_;
  }
  void SetBackgroundAppCount(int count) { app_count_ = count; }
  void SetBackgroundAppCountForProfile(int count) {
    profile_app_count_ = count;
  }
  void SetEnabled(bool enabled) {
    enabled_ = enabled;
    OnBackgroundModeEnabledPrefChanged();
  }
  virtual bool IsBackgroundModePrefEnabled() const OVERRIDE { return enabled_; }

 private:
  bool enabled_;
  int app_count_;
  int profile_app_count_;

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundModeManager);
};

namespace {

void AssertBackgroundModeActive(
    const TestBackgroundModeManager& manager) {
  EXPECT_TRUE(chrome::WillKeepAlive());
  EXPECT_TRUE(manager.HaveStatusTray());
  EXPECT_TRUE(manager.IsLaunchOnStartup());
}

void AssertBackgroundModeInactive(
    const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(chrome::WillKeepAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
  EXPECT_FALSE(manager.IsLaunchOnStartup());
}

void AssertBackgroundModeSuspended(
    const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(chrome::WillKeepAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
  EXPECT_TRUE(manager.IsLaunchOnStartup());
}

} // namespace

class BackgroundModeManagerTest : public testing::Test {
 public:
  BackgroundModeManagerTest() {}
  virtual ~BackgroundModeManagerTest() {}
  virtual void SetUp() OVERRIDE {
    command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
    profile_manager_ = CreateTestingProfileManager();
    profile_ = profile_manager_->CreateTestingProfile("p1");
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

  // From views::MenuModelAdapter::IsCommandEnabled with modification.
  bool IsCommandEnabled(ui::MenuModel* model, int id) const {
    int index = 0;
    if (ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index))
      return model->IsEnabledAt(index);

    return false;
  }

  scoped_ptr<TestingProfileManager> profile_manager_;
  // Test profile used by all tests - this is owned by profile_manager_.
  TestingProfile* profile_;

 private:
  scoped_ptr<TestingProfileManager> CreateTestingProfileManager() {
    scoped_ptr<TestingProfileManager> profile_manager
        (new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    EXPECT_TRUE(profile_manager->SetUp());
    return profile_manager.Pass();
  }

  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManagerTest);
};

class BackgroundModeManagerWithExtensionsTest
    : public BackgroundModeManagerTest {
 public:
  BackgroundModeManagerWithExtensionsTest() {}
  virtual ~BackgroundModeManagerWithExtensionsTest() {}
  virtual void SetUp() OVERRIDE {
    BackgroundModeManagerTest::SetUp();
    // Aura clears notifications from the message center at shutdown.
    message_center::MessageCenter::Initialize();

    // BackgroundModeManager actually affects Chrome start/stop state,
    // tearing down our thread bundle before we've had chance to clean
    // everything up. Keeping Chrome alive prevents this.
    // We aren't interested in if the keep alive works correctly in this test.
    chrome::IncrementKeepAliveCount();

#if defined(OS_CHROMEOS)
    // On ChromeOS shutdown, HandleAppExitingForPlatform will call
    // chrome::DecrementKeepAliveCount because it assumes the aura shell
    // called chrome::IncrementKeepAliveCount. Simulate the call here.
    chrome::IncrementKeepAliveCount();
#endif

    // Create our test BackgroundModeManager.
    manager_.reset(new SimpleTestBackgroundModeManager(
        command_line_.get(), profile_manager_->profile_info_cache()));
    manager_->RegisterProfile(profile_);
  }

  virtual void TearDown() {
    // Clean up the status icon. If this is not done before profile deletes,
    // the context menu updates will DCHECK with the now deleted profiles.
    StatusIcon* status_icon = manager_->status_icon_;
    manager_->status_icon_ = NULL;
    delete status_icon;

    // We have to destroy the profiles now because we created them with real
    // thread state. This causes a lot of machinery to spin up that stops
    // working when we tear down our thread state at the end of the test.
    profile_manager_->DeleteAllTestingProfiles();

    // We're getting ready to shutdown the message loop. Clear everything out!
    base::MessageLoop::current()->RunUntilIdle();
    // Matching the call to IncrementKeepAliveCount in SetUp().
    chrome::DecrementKeepAliveCount();

    // SimpleTestBackgroundModeManager has dependencies on the infrastructure.
    // It should get cleared first.
    manager_.reset();

    // The Profile Manager references the Browser Process.
    // The Browser Process references the Notification UI Manager.
    // The Notification UI Manager references the Message Center.
    // As a result, we have to clear the browser process state here
    // before tearing down the Message Center.
    profile_manager_.reset();

    // Message Center shutdown must occur after the DecrementKeepAliveCount
    // because DecrementKeepAliveCount will end up referencing the message
    // center during cleanup.
    message_center::MessageCenter::Shutdown();

    // Clear the shutdown flag to isolate the remaining effect of this test.
    browser_shutdown::SetTryingToQuit(false);
  }

 protected:
  scoped_ptr<SimpleTestBackgroundModeManager> manager_;

  void AddEphemeralApp(const extensions::Extension* extension,
                       ExtensionService* service) {
    extensions::ExtensionPrefs* prefs =
        extensions::ExtensionPrefs::Get(service->profile());
    ASSERT_TRUE(prefs);
    prefs->OnExtensionInstalled(extension,
                                extensions::Extension::ENABLED,
                                syncer::StringOrdinal(),
                                extensions::kInstallFlagIsEphemeral,
                                std::string());

    service->AddExtension(extension);
  }

 private:
  // Required for extension service.
  content::TestBrowserThreadBundle thread_bundle_;

#if defined(OS_CHROMEOS)
  // ChromeOS needs extra services to run in the following order.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManagerWithExtensionsTest);
};


TEST_F(BackgroundModeManagerTest, BackgroundAppLoadUnload) {
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Mimic app load.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeActive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeSuspended(manager);
  manager.ResumeBackgroundMode();

  // Mimic app unload.
  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Mimic app load while suspended, e.g. from sync. This should enable and
  // resume background mode.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeActive(manager);
}

// App installs while background mode is disabled should do nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  // Turn off background mode.
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Status tray icons will not be created, launch on startup status will not
  // be modified.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  manager.SetBackgroundAppCount(0);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  // Re-enable background mode.
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  AssertBackgroundModeInactive(manager);
}


// App installs while disabled should do nothing until background mode is
// enabled..
TEST_F(BackgroundModeManagerTest, EnableAfterBackgroundAppInstall) {
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  // OnBackgroundAppInstalled does not actually add an app to the
  // BackgroundApplicationListModel which would result in another
  // call to CreateStatusTray.
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile_);
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
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, MultiProfile) {
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.OnApplicationListChanged(profile_);
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
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, ProfileInfoCacheStorage) {
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(chrome::WillKeepAlive());

  ProfileInfoCache* cache = profile_manager_->profile_info_cache();
  EXPECT_EQ(2u, cache->GetNumberOfProfiles());

  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(1));

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.SetBackgroundAppCountForProfile(1);
  manager.OnApplicationListChanged(profile_);

  // Install app for other profile.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.SetBackgroundAppCountForProfile(1);
  manager.OnApplicationListChanged(profile2);

  EXPECT_TRUE(cache->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_TRUE(cache->GetBackgroundStatusOfProfileAtIndex(1));

  manager.SetBackgroundAppCountForProfile(0);
  manager.OnApplicationListChanged(profile_);

  size_t p1_index = cache->GetIndexOfProfileWithPath(profile_->GetPath());
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(p1_index));

  manager.SetBackgroundAppCountForProfile(0);
  manager.OnApplicationListChanged(profile2);

  size_t p2_index = cache->GetIndexOfProfileWithPath(profile_->GetPath());
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(p2_index));

  // Even though neither has background status on, there should still be two
  // profiles in the cache.
  EXPECT_EQ(2u, cache->GetNumberOfProfiles());
}

TEST_F(BackgroundModeManagerTest, ProfileInfoCacheObserver) {
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.SetBackgroundAppCountForProfile(1);
  manager.OnApplicationListChanged(profile_);

  manager.OnProfileNameChanged(
      profile_->GetPath(),
      manager.GetBackgroundModeData(profile_)->name());

  EXPECT_EQ(base::UTF8ToUTF16("p1"),
            manager.GetBackgroundModeData(profile_)->name());

  EXPECT_TRUE(chrome::WillKeepAlive());
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  manager.RegisterProfile(profile2);
  EXPECT_EQ(2, manager.NumberOfBackgroundModeData());

  manager.OnProfileAdded(profile2->GetPath());
  EXPECT_EQ(base::UTF8ToUTF16("p2"),
            manager.GetBackgroundModeData(profile2)->name());

  manager.OnProfileWillBeRemoved(profile2->GetPath());
  // Should still be in background mode after deleting profile.
  EXPECT_TRUE(chrome::WillKeepAlive());
  EXPECT_EQ(1, manager.NumberOfBackgroundModeData());

  // Check that the background mode data we think is in the map actually is.
  EXPECT_EQ(base::UTF8ToUTF16("p1"),
            manager.GetBackgroundModeData(profile_)->name());
}

TEST_F(BackgroundModeManagerTest, DeleteBackgroundProfile) {
  // Tests whether deleting the only profile when it is a BG profile works
  // or not (http://crbug.com/346214).
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Install app, should show status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCount(1);
  manager.SetBackgroundAppCountForProfile(1);
  manager.OnApplicationListChanged(profile_);

  manager.OnProfileNameChanged(
      profile_->GetPath(),
      manager.GetBackgroundModeData(profile_)->name());

  EXPECT_TRUE(chrome::WillKeepAlive());
  manager.SetBackgroundAppCount(0);
  manager.SetBackgroundAppCountForProfile(0);
  manager.OnProfileWillBeRemoved(profile_->GetPath());
  EXPECT_FALSE(chrome::WillKeepAlive());
}

TEST_F(BackgroundModeManagerTest, DisableBackgroundModeUnderTestFlag) {
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_TRUE(manager.ShouldBeInBackgroundMode());
  manager.SetEnabled(false);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerTest,
       BackgroundModeDisabledPreventsKeepAliveOnStartup) {
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  TestBackgroundModeManager manager(
      command_line_.get(), profile_manager_->profile_info_cache(), false);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerWithExtensionsTest, BackgroundMenuGeneration) {
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
      extensions::ExtensionSystem::Get(profile_))->CreateExtensionService(
          CommandLine::ForCurrentProcess(),
          base::FilePath(),
          false);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->Init();

  service->AddComponentExtension(component_extension);
  service->AddComponentExtension(component_extension_with_options);
  service->AddExtension(regular_extension);
  service->AddExtension(regular_extension_with_options);

  scoped_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(NULL));
  scoped_ptr<StatusIconMenuModel> submenu(new StatusIconMenuModel(NULL));
  BackgroundModeManager::BackgroundModeData* bmd =
      manager_->GetBackgroundModeData(profile_);
  bmd->BuildProfileMenu(submenu.get(), menu.get());
  EXPECT_TRUE(
      submenu->GetLabelAt(0) ==
          base::UTF8ToUTF16("Component Extension"));
  EXPECT_FALSE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(0)));
  EXPECT_TRUE(
      submenu->GetLabelAt(1) ==
          base::UTF8ToUTF16("Component Extension with Options"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(1)));
  EXPECT_TRUE(
      submenu->GetLabelAt(2) ==
          base::UTF8ToUTF16("Regular Extension"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(2)));
  EXPECT_TRUE(
      submenu->GetLabelAt(3) ==
          base::UTF8ToUTF16("Regular Extension with Options"));
  EXPECT_TRUE(submenu->IsCommandIdEnabled(submenu->GetCommandIdAt(3)));
}

TEST_F(BackgroundModeManagerWithExtensionsTest,
       BackgroundMenuGenerationMultipleProfile) {
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
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
      extensions::ExtensionSystem::Get(profile_))->CreateExtensionService(
          CommandLine::ForCurrentProcess(),
          base::FilePath(),
          false);
  ExtensionService* service1 =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service1->Init();

  service1->AddComponentExtension(component_extension);
  service1->AddComponentExtension(component_extension_with_options);
  service1->AddExtension(regular_extension);
  service1->AddExtension(regular_extension_with_options);

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile2))->CreateExtensionService(
          CommandLine::ForCurrentProcess(),
          base::FilePath(),
          false);
  ExtensionService* service2 =
      extensions::ExtensionSystem::Get(profile2)->extension_service();
  service2->Init();

  service2->AddComponentExtension(component_extension);
  service2->AddExtension(regular_extension);
  service2->AddExtension(regular_extension_with_options);

  manager_->RegisterProfile(profile2);

  manager_->status_icon_ = new TestStatusIcon();
  manager_->UpdateStatusTrayIconContextMenu();
  StatusIconMenuModel* context_menu = manager_->context_menu_;
  EXPECT_TRUE(context_menu != NULL);

  // Background Profile Enable Checks
  EXPECT_TRUE(context_menu->GetLabelAt(3) == base::UTF8ToUTF16("p1"));
  EXPECT_TRUE(
      context_menu->IsCommandIdEnabled(context_menu->GetCommandIdAt(3)));
  EXPECT_TRUE(context_menu->GetCommandIdAt(3) == 4);

  EXPECT_TRUE(context_menu->GetLabelAt(4) == base::UTF8ToUTF16("p2"));
  EXPECT_TRUE(
      context_menu->IsCommandIdEnabled(context_menu->GetCommandIdAt(4)));
  EXPECT_TRUE(context_menu->GetCommandIdAt(4) == 8);

  // Profile 1 Submenu Checks
  StatusIconMenuModel* profile1_submenu =
      static_cast<StatusIconMenuModel*>(context_menu->GetSubmenuModelAt(3));
  EXPECT_TRUE(
      profile1_submenu->GetLabelAt(0) ==
          base::UTF8ToUTF16("Component Extension"));
  EXPECT_FALSE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(0)));
  EXPECT_TRUE(profile1_submenu->GetCommandIdAt(0) == 0);
  EXPECT_TRUE(
      profile1_submenu->GetLabelAt(1) ==
          base::UTF8ToUTF16("Component Extension with Options"));
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(1)));
  EXPECT_TRUE(profile1_submenu->GetCommandIdAt(1) == 1);
  EXPECT_TRUE(
      profile1_submenu->GetLabelAt(2) ==
          base::UTF8ToUTF16("Regular Extension"));
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(2)));
  EXPECT_TRUE(profile1_submenu->GetCommandIdAt(2) == 2);
  EXPECT_TRUE(
      profile1_submenu->GetLabelAt(3) ==
          base::UTF8ToUTF16("Regular Extension with Options"));
  EXPECT_TRUE(
      profile1_submenu->IsCommandIdEnabled(
          profile1_submenu->GetCommandIdAt(3)));
  EXPECT_TRUE(profile1_submenu->GetCommandIdAt(3) == 3);

  // Profile 2 Submenu Checks
  StatusIconMenuModel* profile2_submenu =
      static_cast<StatusIconMenuModel*>(context_menu->GetSubmenuModelAt(4));
  EXPECT_TRUE(
      profile2_submenu->GetLabelAt(0) ==
          base::UTF8ToUTF16("Component Extension"));
  EXPECT_FALSE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(0)));
  EXPECT_TRUE(profile2_submenu->GetCommandIdAt(0) == 5);
  EXPECT_TRUE(
      profile2_submenu->GetLabelAt(1) ==
          base::UTF8ToUTF16("Regular Extension"));
  EXPECT_TRUE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(1)));
  EXPECT_TRUE(profile2_submenu->GetCommandIdAt(1) == 6);
  EXPECT_TRUE(
      profile2_submenu->GetLabelAt(2) ==
          base::UTF8ToUTF16("Regular Extension with Options"));
  EXPECT_TRUE(
      profile2_submenu->IsCommandIdEnabled(
          profile2_submenu->GetCommandIdAt(2)));
  EXPECT_TRUE(profile2_submenu->GetCommandIdAt(2) == 7);

  // Model Adapter Checks for crbug.com/315164
  // P1: Profile 1 Menu Item
  // P2: Profile 2 Menu Item
  // CE: Component Extension Menu Item
  // CEO: Component Extenison with Options Menu Item
  // RE: Regular Extension Menu Item
  // REO: Regular Extension with Options Menu Item
  EXPECT_FALSE(IsCommandEnabled(context_menu, 0)); // P1 - CE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 1));  // P1 - CEO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 2));  // P1 - RE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 3));  // P1 - REO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 4));  // P1
  EXPECT_FALSE(IsCommandEnabled(context_menu, 5)); // P2 - CE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 6));  // P2 - RE
  EXPECT_TRUE(IsCommandEnabled(context_menu, 7));  // P2 - REO
  EXPECT_TRUE(IsCommandEnabled(context_menu, 8));  // P2
}

TEST_F(BackgroundModeManagerWithExtensionsTest, BalloonDisplay) {
  scoped_refptr<extensions::Extension> bg_ext(
    CreateExtension(
        extensions::Manifest::COMMAND_LINE,
        "{\"name\": \"Background Extension\", "
        "\"version\": \"1.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"background\"]}",
        "ID-1"));

  scoped_refptr<extensions::Extension> upgraded_bg_ext(
    CreateExtension(
        extensions::Manifest::COMMAND_LINE,
        "{\"name\": \"Background Extension\", "
        "\"version\": \"2.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"background\"]}",
        "ID-1"));

  scoped_refptr<extensions::Extension> no_bg_ext(
    CreateExtension(
        extensions::Manifest::COMMAND_LINE,
        "{\"name\": \"Regular Extension\", "
        "\"version\": \"1.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": []}",
        "ID-2"));

  scoped_refptr<extensions::Extension> upgraded_no_bg_ext_has_bg(
    CreateExtension(
        extensions::Manifest::COMMAND_LINE,
        "{\"name\": \"Regular Extension\", "
        "\"version\": \"2.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"background\"]}",
        "ID-2"));

  scoped_refptr<extensions::Extension> ephemeral_app(
    CreateExtension(
        extensions::Manifest::COMMAND_LINE,
        "{\"name\": \"Ephemeral App\", "
        "\"version\": \"1.0\","
        "\"manifest_version\": 2,"
        "\"permissions\": [\"pushMessaging\"]}",
        "ID-3"));

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_))->CreateExtensionService(
          CommandLine::ForCurrentProcess(),
          base::FilePath(),
          false);

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(!service->is_ready());
  service->Init();
  DCHECK(service->is_ready());
  manager_->status_icon_ = new TestStatusIcon();
  manager_->UpdateStatusTrayIconContextMenu();

  // Adding a background extension should show the balloon.
  EXPECT_FALSE(manager_->HasShownBalloon());
  service->AddExtension(bg_ext);
  EXPECT_TRUE(manager_->HasShownBalloon());

  // Adding an extension without background should not show the balloon.
  manager_->SetHasShownBalloon(false);
  service->AddExtension(no_bg_ext);
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Upgrading an extension that has background should not reshow the balloon.
  service->AddExtension(upgraded_bg_ext);
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Upgrading an extension that didn't have background to one that does should
  // show the balloon.
  service->AddExtension(upgraded_no_bg_ext_has_bg);
  EXPECT_TRUE(manager_->HasShownBalloon());

  // Installing an ephemeral app should not show the balloon.
  manager_->SetHasShownBalloon(false);
  AddEphemeralApp(ephemeral_app.get(), service);
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Promoting the ephemeral app to a regular installed app should now show
  // the balloon.
  service->PromoteEphemeralApp(ephemeral_app.get(), false /*from sync*/);
  EXPECT_TRUE(manager_->HasShownBalloon());
}

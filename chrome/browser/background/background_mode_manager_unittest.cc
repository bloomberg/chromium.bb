// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
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
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using testing::_;
using testing::AtMost;
using testing::Exactly;
using testing::InSequence;
using testing::Mock;
using testing::StrictMock;

namespace {

// Helper class that tracks state transitions in BackgroundModeManager and
// exposes them via getters (or gmock for EnableLaunchOnStartup).
class TestBackgroundModeManager : public StrictMock<BackgroundModeManager> {
 public:
  TestBackgroundModeManager(const base::CommandLine& command_line,
                            ProfileInfoCache* cache)
      : StrictMock<BackgroundModeManager>(command_line, cache),
        have_status_tray_(false),
        has_shown_balloon_(false) {
    ResumeBackgroundMode();
  }

  MOCK_METHOD1(EnableLaunchOnStartup, void(bool should_launch));

  // TODO: Use strict-mocking rather than keeping state through overrides below.
  void DisplayAppInstalledNotification(
      const extensions::Extension* extension) override {
    has_shown_balloon_ = true;
  }
  void CreateStatusTrayIcon() override { have_status_tray_ = true; }
  void RemoveStatusTrayIcon() override { have_status_tray_ = false; }

  bool HaveStatusTray() const { return have_status_tray_; }
  bool HasShownBalloon() const { return has_shown_balloon_; }
  void SetHasShownBalloon(bool value) { has_shown_balloon_ = value; }

 private:
  // Flags to track whether we have a status tray/have shown the balloon.
  bool have_status_tray_;
  bool has_shown_balloon_;

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundModeManager);
};

class TestStatusIcon : public StatusIcon {
 public:
  TestStatusIcon() {}
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const base::string16& tool_tip) override {}
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const base::string16& title,
                      const base::string16& contents) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStatusIcon);
};

void AssertBackgroundModeActive(const TestBackgroundModeManager& manager) {
  EXPECT_TRUE(chrome::WillKeepAlive());
  EXPECT_TRUE(manager.HaveStatusTray());
}

void AssertBackgroundModeInactive(const TestBackgroundModeManager& manager) {
  EXPECT_FALSE(chrome::WillKeepAlive());
  EXPECT_FALSE(manager.HaveStatusTray());
}

}  // namespace

// More complex test helper that exposes APIs for fine grained control of
// things like the number of background applications. This allows writing
// smaller tests that don't have to install/uninstall extensions.
class AdvancedTestBackgroundModeManager : public TestBackgroundModeManager {
 public:
  AdvancedTestBackgroundModeManager(const base::CommandLine& command_line,
                                    ProfileInfoCache* cache,
                                    bool enabled)
      : TestBackgroundModeManager(command_line, cache), enabled_(enabled) {}

  int GetBackgroundAppCount() const override {
    int app_count = 0;
    for (const auto& profile_count_pair : profile_app_counts_)
      app_count += profile_count_pair.second;
    return app_count;
  }
  int GetBackgroundAppCountForProfile(Profile* const profile) const override {
    auto it = profile_app_counts_.find(profile);
    if (it == profile_app_counts_.end()) {
      ADD_FAILURE();
      return 0;
    }
    return it->second;
  }
  void SetBackgroundAppCountForProfile(Profile* profile, int count) {
    profile_app_counts_[profile] = count;
  }
  void SetEnabled(bool enabled) {
    enabled_ = enabled;
    OnBackgroundModeEnabledPrefChanged();
  }
  bool IsBackgroundModePrefEnabled() const override { return enabled_; }

 private:
  bool enabled_;
  std::map<Profile*, int> profile_app_counts_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedTestBackgroundModeManager);
};

class BackgroundModeManagerTest : public testing::Test {
 public:
  BackgroundModeManagerTest() {}
  ~BackgroundModeManagerTest() override {}

  void SetUp() override {
    command_line_.reset(new base::CommandLine(base::CommandLine::NO_PROGRAM));
    profile_manager_ = CreateTestingProfileManager();
    profile_ = profile_manager_->CreateTestingProfile("p1");
  }

 protected:
  scoped_refptr<extensions::Extension> CreateExtension(
      extensions::Manifest::Location location,
      const std::string& data,
      const std::string& id) {
    scoped_ptr<base::DictionaryValue> parsed_manifest(
        extensions::api_test_utils::ParseDictionary(data));
    return extensions::api_test_utils::CreateExtension(
        location, parsed_manifest.get(), id);
  }

  // From views::MenuModelAdapter::IsCommandEnabled with modification.
  bool IsCommandEnabled(ui::MenuModel* model, int id) const {
    int index = 0;
    if (ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index))
      return model->IsEnabledAt(index);

    return false;
  }

  scoped_ptr<base::CommandLine> command_line_;

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
  ~BackgroundModeManagerWithExtensionsTest() override {}

  void SetUp() override {
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
    manager_.reset(new TestBackgroundModeManager(
        *command_line_, profile_manager_->profile_info_cache()));
    manager_->RegisterProfile(profile_);
  }

  void TearDown() override {
    // Clean up the status icon. If this is not done before profile deletes,
    // the context menu updates will DCHECK with the now deleted profiles.
    StatusIcon* status_icon = manager_->status_icon_;
    manager_->status_icon_ = NULL;
    delete status_icon;

    // We have to destroy the profiles now because we created them with real
    // thread state. This causes a lot of machinery to spin up that stops
    // working when we tear down our thread state at the end of the test.
    // Deleting our testing profile may have the side-effect of disabling
    // background mode if it was enabled for that profile (explicitly note that
    // here to satisfy StrictMock requirements.
    EXPECT_CALL(*manager_, EnableLaunchOnStartup(false)).Times(AtMost(1));
    profile_manager_->DeleteAllTestingProfiles();
    Mock::VerifyAndClearExpectations(manager_.get());

    // We're getting ready to shutdown the message loop. Clear everything out!
    base::MessageLoop::current()->RunUntilIdle();
    // Matching the call to IncrementKeepAliveCount in SetUp().
    chrome::DecrementKeepAliveCount();

    // TestBackgroundModeManager has dependencies on the infrastructure.
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

  scoped_ptr<TestBackgroundModeManager> manager_;

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
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Mimic app load.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);
  manager.ResumeBackgroundMode();

  // Mimic app unload.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundAppCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);

  manager.SuspendBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Mimic app load while suspended, e.g. from sync. This should enable and
  // resume background mode.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);
}

// App installs while background mode is disabled should do nothing.
TEST_F(BackgroundModeManagerTest, BackgroundAppInstallUninstallWhileDisabled) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);

  // Turn off background mode (shouldn't explicitly disable launch-on-startup as
  // the app-count is zero and launch-on-startup shouldn't be considered on).
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  AssertBackgroundModeInactive(manager);

  // Status tray icons will not be created, launch on startup status will not
  // be modified.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  manager.SetBackgroundAppCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeInactive(manager);

  // Re-enable background mode (shouldn't actually enable launch-on-startup as
  // the app-count is zero).
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  AssertBackgroundModeInactive(manager);
}


// App installs while disabled should do nothing until background mode is
// enabled..
TEST_F(BackgroundModeManagerTest, EnableAfterBackgroundAppInstall) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundAppInstalled(NULL);
  // OnBackgroundAppInstalled does not actually add an app to the
  // BackgroundApplicationListModel which would result in another
  // call to CreateStatusTray.
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  AssertBackgroundModeActive(manager);
  Mock::VerifyAndClearExpectations(&manager);

  // Turn off background mode - should hide status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);

  // Turn back on background mode - again, no status tray icon
  // will show up since we didn't actually add anything to the list.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  // Uninstall app, should hide status tray icon again.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundAppCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, MultiProfile) {
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  // Install app for other profile, should show other status tray icon.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile2, 2);
  manager.OnApplicationListChanged(profile2);
  AssertBackgroundModeActive(manager);

  // Should hide both status tray icons.
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetEnabled(false);
  manager.DisableBackgroundMode();
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);

  // Turn back on background mode - should show both status tray icons.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.SetEnabled(true);
  manager.EnableBackgroundMode();
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeActive(manager);

  manager.SetBackgroundAppCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);
  manager.SetBackgroundAppCountForProfile(profile2, 1);
  manager.OnApplicationListChanged(profile2);
  // There is still one background app alive
  AssertBackgroundModeActive(manager);
  // Verify the implicit expectations of no calls on this StrictMock.
  Mock::VerifyAndClearExpectations(&manager);

  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundAppCountForProfile(profile2, 0);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);
  AssertBackgroundModeInactive(manager);
}

TEST_F(BackgroundModeManagerTest, ProfileInfoCacheStorage) {
  TestingProfile* profile2 = profile_manager_->CreateTestingProfile("p2");
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  manager.RegisterProfile(profile2);
  EXPECT_FALSE(chrome::WillKeepAlive());

  ProfileInfoCache* cache = profile_manager_->profile_info_cache();
  EXPECT_EQ(2u, cache->GetNumberOfProfiles());

  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(1));

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  // Install app for other profile.
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile2, 1);
  manager.OnApplicationListChanged(profile2);

  EXPECT_TRUE(cache->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_TRUE(cache->GetBackgroundStatusOfProfileAtIndex(1));

  manager.SetBackgroundAppCountForProfile(profile_, 0);
  manager.OnApplicationListChanged(profile_);

  size_t p1_index = cache->GetIndexOfProfileWithPath(profile_->GetPath());
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(p1_index));

  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetBackgroundAppCountForProfile(profile2, 0);
  manager.OnApplicationListChanged(profile2);
  Mock::VerifyAndClearExpectations(&manager);

  size_t p2_index = cache->GetIndexOfProfileWithPath(profile_->GetPath());
  EXPECT_FALSE(cache->GetBackgroundStatusOfProfileAtIndex(p2_index));

  // Even though neither has background status on, there should still be two
  // profiles in the cache.
  EXPECT_EQ(2u, cache->GetNumberOfProfiles());
}

TEST_F(BackgroundModeManagerTest, ProfileInfoCacheObserver) {
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  // Background mode should remain active for the remainder of this test.

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
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_FALSE(chrome::WillKeepAlive());

  // Install app, should show status tray icon.
  EXPECT_CALL(manager, EnableLaunchOnStartup(true)).Times(Exactly(1));
  manager.OnBackgroundAppInstalled(NULL);
  manager.SetBackgroundAppCountForProfile(profile_, 1);
  manager.OnApplicationListChanged(profile_);
  Mock::VerifyAndClearExpectations(&manager);

  manager.OnProfileNameChanged(
      profile_->GetPath(),
      manager.GetBackgroundModeData(profile_)->name());

  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  EXPECT_TRUE(chrome::WillKeepAlive());
  manager.SetBackgroundAppCountForProfile(profile_, 0);
  manager.OnProfileWillBeRemoved(profile_->GetPath());
  Mock::VerifyAndClearExpectations(&manager);
  EXPECT_FALSE(chrome::WillKeepAlive());
}

TEST_F(BackgroundModeManagerTest, DisableBackgroundModeUnderTestFlag) {
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), true);
  manager.RegisterProfile(profile_);
  EXPECT_TRUE(manager.ShouldBeInBackgroundMode());

  // No enable-launch-on-startup calls expected yet.
  Mock::VerifyAndClearExpectations(&manager);
  EXPECT_CALL(manager, EnableLaunchOnStartup(false)).Times(Exactly(1));
  manager.SetEnabled(false);
  EXPECT_FALSE(manager.ShouldBeInBackgroundMode());
}

TEST_F(BackgroundModeManagerTest,
       BackgroundModeDisabledPreventsKeepAliveOnStartup) {
  command_line_->AppendSwitch(switches::kKeepAliveForTest);
  AdvancedTestBackgroundModeManager manager(
      *command_line_, profile_manager_->profile_info_cache(), false);
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
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->Init();

  EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  service->AddComponentExtension(component_extension.get());
  service->AddComponentExtension(component_extension_with_options.get());
  service->AddExtension(regular_extension.get());
  service->AddExtension(regular_extension_with_options.get());
  Mock::VerifyAndClearExpectations(manager_.get());

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
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);
  ExtensionService* service1 =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service1->Init();

  EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  service1->AddComponentExtension(component_extension.get());
  service1->AddComponentExtension(component_extension_with_options.get());
  service1->AddExtension(regular_extension.get());
  service1->AddExtension(regular_extension_with_options.get());
  Mock::VerifyAndClearExpectations(manager_.get());

  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile2))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);
  ExtensionService* service2 =
      extensions::ExtensionSystem::Get(profile2)->extension_service();
  service2->Init();

  service2->AddComponentExtension(component_extension.get());
  service2->AddExtension(regular_extension.get());
  service2->AddExtension(regular_extension_with_options.get());

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
      extensions::ExtensionSystem::Get(profile_))
      ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), false);

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  ASSERT_FALSE(service->is_ready());
  service->Init();

  ASSERT_TRUE(service->is_ready());
  manager_->status_icon_ = new TestStatusIcon();
  manager_->UpdateStatusTrayIconContextMenu();

  // Adding a background extension should show the balloon.
  EXPECT_FALSE(manager_->HasShownBalloon());
  EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  service->AddExtension(bg_ext.get());
  Mock::VerifyAndClearExpectations(manager_.get());
  EXPECT_TRUE(manager_->HasShownBalloon());

  // Adding an extension without background should not show the balloon.
  manager_->SetHasShownBalloon(false);
  service->AddExtension(no_bg_ext.get());
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Upgrading an extension that has background should not reshow the balloon.
  {
    // TODO: Fix crbug.com/438376 and remove these checks.
    InSequence expected_call_sequence;
    EXPECT_CALL(*manager_, EnableLaunchOnStartup(false)).Times(Exactly(1));
    EXPECT_CALL(*manager_, EnableLaunchOnStartup(true)).Times(Exactly(1));
  }
  service->AddExtension(upgraded_bg_ext.get());
  Mock::VerifyAndClearExpectations(manager_.get());
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Upgrading an extension that didn't have background to one that does should
  // show the balloon.
  service->AddExtension(upgraded_no_bg_ext_has_bg.get());
  EXPECT_TRUE(manager_->HasShownBalloon());

  // Installing an ephemeral app should not show the balloon.
  manager_->SetHasShownBalloon(false);
  AddEphemeralApp(ephemeral_app.get(), service);
  EXPECT_FALSE(manager_->HasShownBalloon());

  // Promoting the ephemeral app to a regular installed app should now show
  // the balloon.
  service->PromoteEphemeralApp(ephemeral_app.get(), false /* from sync */);
  EXPECT_TRUE(manager_->HasShownBalloon());
}

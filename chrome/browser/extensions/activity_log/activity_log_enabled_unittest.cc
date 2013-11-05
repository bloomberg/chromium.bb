// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_builder.h"

#if defined OS_CHROMEOS
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

class ActivityLogEnabledTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
  }

  virtual void TearDown() OVERRIDE {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(ActivityLogEnabledTest, NoSwitch) {
  scoped_ptr<TestingProfile> profile(
    static_cast<TestingProfile*>(CreateBrowserContext()));
  EXPECT_FALSE(
      profile->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));

  ActivityLog* activity_log = ActivityLog::GetInstance(profile.get());

  EXPECT_FALSE(
    profile->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log->IsWatchdogAppActive());
}

TEST_F(ActivityLogEnabledTest, CommandLineSwitch) {
  scoped_ptr<TestingProfile> profile1(
    static_cast<TestingProfile*>(CreateBrowserContext()));
  scoped_ptr<TestingProfile> profile2(
    static_cast<TestingProfile*>(CreateBrowserContext()));

  CommandLine command_line(CommandLine::NO_PROGRAM);
  CommandLine saved_cmdline_ = *CommandLine::ForCurrentProcess();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExtensionActivityLogging);
  ActivityLog* activity_log1 = ActivityLog::GetInstance(profile1.get());
  *CommandLine::ForCurrentProcess() = saved_cmdline_;
  ActivityLog* activity_log2 = ActivityLog::GetInstance(profile2.get());

  EXPECT_FALSE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
}

TEST_F(ActivityLogEnabledTest, PrefSwitch) {
  scoped_ptr<TestingProfile> profile1(
    static_cast<TestingProfile*>(CreateBrowserContext()));
  scoped_ptr<TestingProfile> profile2(
    static_cast<TestingProfile*>(CreateBrowserContext()));

  EXPECT_FALSE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));

  profile1->GetPrefs()->SetBoolean(prefs::kWatchdogExtensionActive, true);
  ActivityLog* activity_log1 = ActivityLog::GetInstance(profile1.get());
  ActivityLog* activity_log2 = ActivityLog::GetInstance(profile2.get());

  EXPECT_TRUE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());
}

TEST_F(ActivityLogEnabledTest, WatchdogSwitch) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  scoped_ptr<TestingProfile> profile1(
    static_cast<TestingProfile*>(CreateBrowserContext()));
  scoped_ptr<TestingProfile> profile2(
    static_cast<TestingProfile*>(CreateBrowserContext()));
  // Extension service is destroyed by the profile.
  ExtensionService* extension_service1 =
    static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile1.get()))->CreateExtensionService(
            &command_line, base::FilePath(), false);
  static_cast<TestExtensionSystem*>(
      ExtensionSystem::Get(profile1.get()))->SetReady();

  ActivityLog* activity_log1 = ActivityLog::GetInstance(profile1.get());
  ActivityLog* activity_log2 = ActivityLog::GetInstance(profile2.get());

  // Allow Activity Log to install extension tracker.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));

  scoped_refptr<Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Watchdog Extension ")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .SetID(kActivityLogExtensionId)
          .Build();
  extension_service1->AddExtension(extension.get());

  EXPECT_TRUE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());

  extension_service1->DisableExtension(kActivityLogExtensionId,
                                       Extension::DISABLE_USER_ACTION);

  EXPECT_FALSE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());

  extension_service1->EnableExtension(kActivityLogExtensionId);

  EXPECT_TRUE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_TRUE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());

  extension_service1->UninstallExtension(kActivityLogExtensionId, false, NULL);

  EXPECT_FALSE(
      profile1->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(
      profile2->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log1->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log2->IsWatchdogAppActive());
  EXPECT_FALSE(activity_log1->IsDatabaseEnabled());
  EXPECT_FALSE(activity_log2->IsDatabaseEnabled());
}

TEST_F(ActivityLogEnabledTest, AppAndCommandLine) {
  // Set the command line switch.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  CommandLine saved_cmdline_ = *CommandLine::ForCurrentProcess();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExtensionActivityLogging);

  scoped_ptr<TestingProfile> profile(
    static_cast<TestingProfile*>(CreateBrowserContext()));
  // Extension service is destroyed by the profile.
  ExtensionService* extension_service =
    static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile.get()))->CreateExtensionService(
            &command_line, base::FilePath(), false);
  static_cast<TestExtensionSystem*>(
      ExtensionSystem::Get(profile.get()))->SetReady();

  ActivityLog* activity_log = ActivityLog::GetInstance(profile.get());
  // Allow Activity Log to install extension tracker.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(activity_log->IsDatabaseEnabled());
  EXPECT_FALSE(
      profile->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log->IsWatchdogAppActive());

  // Enable the extension.
  scoped_refptr<Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Watchdog Extension ")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .SetID(kActivityLogExtensionId)
          .Build();
  extension_service->AddExtension(extension.get());

  EXPECT_TRUE(activity_log->IsDatabaseEnabled());
  EXPECT_TRUE(
      profile->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_TRUE(activity_log->IsWatchdogAppActive());

  extension_service->UninstallExtension(kActivityLogExtensionId, false, NULL);

  EXPECT_TRUE(activity_log->IsDatabaseEnabled());
  EXPECT_FALSE(
      profile->GetPrefs()->GetBoolean(prefs::kWatchdogExtensionActive));
  EXPECT_FALSE(activity_log->IsWatchdogAppActive());

  // Cleanup.
  *CommandLine::ForCurrentProcess() = saved_cmdline_;
}

} // namespace extensions

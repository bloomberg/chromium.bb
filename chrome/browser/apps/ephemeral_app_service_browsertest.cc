// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/apps/ephemeral_app_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest.h"

using extensions::PlatformAppBrowserTest;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionSystem;

namespace {

const int kNumTestApps = 2;
const char* kTestApps[] = {
  "platform_apps/shell_window/generic",
  "platform_apps/minimal"
};

}  // namespace

class EphemeralAppServiceBrowserTest : public PlatformAppBrowserTest {
 protected:
  void LoadApps() {
    for (int i = 0; i < kNumTestApps; ++i) {
      base::FilePath path = test_data_dir_.AppendASCII(kTestApps[i]);
      const Extension* extension =
          InstallExtensionWithSourceAndFlags(
              path,
              1,
              extensions::Manifest::UNPACKED,
              Extension::IS_EPHEMERAL);
      app_ids_.push_back(extension->id());
    }

    ASSERT_EQ(kNumTestApps, (int) app_ids_.size());
  }

  void GarbageCollectEphemeralApps() {
    EphemeralAppService* ephemeral_service = EphemeralAppService::Get(
        browser()->profile());
    ASSERT_TRUE(ephemeral_service);
    ephemeral_service->GarbageCollectApps();
  }

  std::vector<std::string> app_ids_;
};

// Verifies that inactive ephemeral apps are uninstalled and active apps are
// not removed. Extensive testing of the ephemeral app cache's replacement
// policies is done in the unit tests for EphemeralAppService. This is more
// like an integration test.
IN_PROC_BROWSER_TEST_F(EphemeralAppServiceBrowserTest,
                       GarbageCollectInactiveApps) {
  LoadApps();

  const base::Time time_now = base::Time::Now();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);

  // Set launch time for an inactive app.
  std::string inactive_app_id = app_ids_[0];
  base::Time inactive_launch = time_now -
      base::TimeDelta::FromDays(EphemeralAppService::kAppInactiveThreshold + 1);
  prefs->SetLastLaunchTime(inactive_app_id, inactive_launch);

  // Set launch time for an active app.
  std::string active_app_id = app_ids_[1];
  base::Time active_launch = time_now -
      base::TimeDelta::FromDays(EphemeralAppService::kAppKeepThreshold);
  prefs->SetLastLaunchTime(active_app_id, active_launch);

  // Perform garbage collection.
  content::WindowedNotificationObserver uninstall_signal(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(browser()->profile()));
  GarbageCollectEphemeralApps();
  uninstall_signal.Wait();

  ExtensionService* service = ExtensionSystem::Get(browser()->profile())
      ->extension_service();
  EXPECT_TRUE(service);
  EXPECT_FALSE(service->GetInstalledExtension(inactive_app_id));
  EXPECT_TRUE(service->GetInstalledExtension(active_app_id));
}

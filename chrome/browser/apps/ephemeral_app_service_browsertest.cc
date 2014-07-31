// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/apps/ephemeral_app_browsertest.h"
#include "chrome/browser/apps/ephemeral_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/manifest.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;

namespace {

const int kNumTestApps = 2;
const char* kTestApps[] = {
  "app_window/generic",
  "minimal"
};

}  // namespace

class EphemeralAppServiceBrowserTest : public EphemeralAppTestBase {
 protected:
  void LoadApps() {
    for (int i = 0; i < kNumTestApps; ++i) {
      const Extension* extension = InstallEphemeralApp(kTestApps[i]);
      ASSERT_TRUE(extension);
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

  void InitEphemeralAppCount(EphemeralAppService* ephemeral_service) {
    ephemeral_service->InitEphemeralAppCount();
  }

  std::vector<std::string> app_ids_;
};

// Verifies that inactive ephemeral apps are uninstalled and active apps are
// not removed. Extensive testing of the ephemeral app cache's replacement
// policies is done in the unit tests for EphemeralAppService. This is more
// like an integration test.
IN_PROC_BROWSER_TEST_F(EphemeralAppServiceBrowserTest,
                       GarbageCollectInactiveApps) {
  EphemeralAppService* ephemeral_service =
      EphemeralAppService::Get(browser()->profile());
  ASSERT_TRUE(ephemeral_service);
  InitEphemeralAppCount(ephemeral_service);

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
      extensions::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
      content::Source<Profile>(browser()->profile()));
  GarbageCollectEphemeralApps();
  uninstall_signal.Wait();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry);
  EXPECT_FALSE(registry->GetExtensionById(inactive_app_id,
                                          ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(
      registry->GetExtensionById(active_app_id, ExtensionRegistry::EVERYTHING));

  EXPECT_EQ(1, ephemeral_service->ephemeral_app_count());
}

// Verify that the count of ephemeral apps is maintained correctly.
IN_PROC_BROWSER_TEST_F(EphemeralAppServiceBrowserTest, EphemeralAppCount) {
  EphemeralAppService* ephemeral_service =
      EphemeralAppService::Get(browser()->profile());
  ASSERT_TRUE(ephemeral_service);
  InitEphemeralAppCount(ephemeral_service);

  // The count should not increase for regular installed apps.
  EXPECT_TRUE(InstallPlatformApp("minimal"));
  EXPECT_EQ(0, ephemeral_service->ephemeral_app_count());

  // The count should increase when an ephemeral app is added.
  const Extension* app = InstallEphemeralApp(kMessagingReceiverApp);
  ASSERT_TRUE(app);
  EXPECT_EQ(1, ephemeral_service->ephemeral_app_count());

  // The count should remain constant if the ephemeral app is updated.
  const std::string app_id = app->id();
  app = UpdateEphemeralApp(
      app_id, GetTestPath(kMessagingReceiverAppV2),
      GetTestPath(kMessagingReceiverApp).ReplaceExtension(
          FILE_PATH_LITERAL(".pem")));
  ASSERT_TRUE(app);
  EXPECT_EQ(1, ephemeral_service->ephemeral_app_count());

  // The count should decrease when an ephemeral app is promoted to a regular
  // installed app.
  PromoteEphemeralApp(app);
  EXPECT_EQ(0, ephemeral_service->ephemeral_app_count());
}

// Verify that the cache of ephemeral apps is correctly cleared. Running apps
// should not be removed.
IN_PROC_BROWSER_TEST_F(EphemeralAppServiceBrowserTest, ClearCachedApps) {
  const Extension* running_app =
      InstallAndLaunchEphemeralApp(kMessagingReceiverApp);
  const Extension* inactive_app =
      InstallAndLaunchEphemeralApp(kDispatchEventTestApp);
  std::string inactive_app_id = inactive_app->id();
  std::string running_app_id = running_app->id();
  CloseApp(inactive_app_id);

  EphemeralAppService* ephemeral_service =
      EphemeralAppService::Get(browser()->profile());
  ASSERT_TRUE(ephemeral_service);
  EXPECT_EQ(2, ephemeral_service->ephemeral_app_count());

  ephemeral_service->ClearCachedApps();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry);
  EXPECT_FALSE(registry->GetExtensionById(inactive_app_id,
                                          ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(registry->GetExtensionById(running_app_id,
                                         ExtensionRegistry::EVERYTHING));

  EXPECT_EQ(1, ephemeral_service->ephemeral_app_count());
}

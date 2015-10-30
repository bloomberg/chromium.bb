// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/apps/ephemeral_app_browsertest.h"
#include "chrome/browser/apps/ephemeral_app_service.h"
#include "extensions/browser/extension_registry.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

class EphemeralAppServiceBrowserTest : public EphemeralAppTestBase {};

// Verify that the cache of ephemeral apps is correctly cleared. All ephemeral
// apps should be removed.
IN_PROC_BROWSER_TEST_F(EphemeralAppServiceBrowserTest, ClearCachedApps) {
  const Extension* running_app =
      InstallAndLaunchEphemeralApp(kMessagingReceiverApp);
  const Extension* inactive_app =
      InstallAndLaunchEphemeralApp(kDispatchEventTestApp);
  std::string inactive_app_id = inactive_app->id();
  std::string running_app_id = running_app->id();
  CloseAppWaitForUnload(inactive_app_id);

  EphemeralAppService* ephemeral_service =
      EphemeralAppService::Get(browser()->profile());
  ASSERT_TRUE(ephemeral_service);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry);

  int extension_count = registry->GenerateInstalledExtensionsSet()->size();

  ephemeral_service->ClearCachedApps();

  EXPECT_FALSE(registry->GetExtensionById(inactive_app_id,
                                          ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(registry->GetExtensionById(running_app_id,
                                          ExtensionRegistry::EVERYTHING));

  int new_extension_count = registry->GenerateInstalledExtensionsSet()->size();
  EXPECT_EQ(2, extension_count - new_extension_count);
}

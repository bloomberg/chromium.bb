// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class ExtensionContextMenuModelTest : public ExtensionServiceTestBase {
};

// Tests that applicable menu items are disabled when a ManagementPolicy
// prohibits them.
TEST_F(ExtensionContextMenuModelTest, PolicyDisablesItems) {
  InitializeEmptyExtensionService();
  // Build an extension to pass to the menu constructor.  It needs an
  // ExtensionAction.
  scoped_refptr<Extension> extension = ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "Page Action Extension")
                   .Set("version", "1")
                   .Set("manifest_version", 2)
                   .Set("page_action", DictionaryBuilder()
                        .Set("default_title", "Hello")))
      .Build();
  ASSERT_TRUE(extension.get());
  service_->AddExtension(extension.get());

  // Create a Browser for the ExtensionContextMenuModel to use.
  Browser::CreateParams params(profile_.get(), chrome::GetActiveDesktop());
  TestBrowserWindow test_window;
  params.window = &test_window;
  Browser browser(params);

  scoped_refptr<ExtensionContextMenuModel> menu(
      new ExtensionContextMenuModel(extension.get(), &browser, NULL));

  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_.get());
  system->management_policy()->UnregisterAllProviders();

  // Actions should be enabled.
  ASSERT_TRUE(menu->IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));

  extensions::TestManagementPolicyProvider policy_provider(
    extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  system->management_policy()->RegisterProvider(&policy_provider);

  // Now the actions are disabled.
  ASSERT_FALSE(menu->IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));

  // Don't leave |policy_provider| dangling.
  system->management_policy()->UnregisterAllProviders();
}

}  // namespace
}  // namespace extensions

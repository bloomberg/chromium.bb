// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExtensionContextMenuModelTest : public ExtensionServiceTestBase {
 public:
  ExtensionContextMenuModelTest();

  // Build an extension to pass to the menu constructor.  It needs an
  // ExtensionAction.
  scoped_refptr<Extension> BuildExtension();

  // Creates an extension menu item for |extension| with the given |context|
  // and adds it to |manager|. Refreshes |model| to show new item.
  void AddContextItemAndRefreshModel(MenuManager* manager,
                                     Extension* extension,
                                     MenuItem::Context context,
                                     ExtensionContextMenuModel* model);

  // Reinitializes the given |model|.
  void RefreshMenu(ExtensionContextMenuModel* model);

  // Returns the number of extension menu items that show up in |model|.
  int CountExtensionItems(ExtensionContextMenuModel* model);

 private:
  int cur_id_;
};

ExtensionContextMenuModelTest::ExtensionContextMenuModelTest()
    : cur_id_(0) {}

scoped_refptr<Extension> ExtensionContextMenuModelTest::BuildExtension() {
  return ExtensionBuilder()
      .SetManifest(
           DictionaryBuilder()
               .Set("name", "Page Action Extension")
               .Set("version", "1")
               .Set("manifest_version", 2)
               .Set("page_action",
                    DictionaryBuilder().Set("default_title", "Hello")))
      .Build();
}

void ExtensionContextMenuModelTest::AddContextItemAndRefreshModel(
    MenuManager* manager,
    Extension* extension,
    MenuItem::Context context,
    ExtensionContextMenuModel* model) {
  MenuItem::Type type = MenuItem::NORMAL;
  MenuItem::ContextList contexts(context);
  const MenuItem::ExtensionKey key(extension->id());
  MenuItem::Id id(false, key);
  id.uid = ++cur_id_;
  manager->AddContextItem(extension, new MenuItem(id,
                                                  "test",
                                                  false,  // checked
                                                  true,  // enabled
                                                  type,
                                                  contexts));
  RefreshMenu(model);
}

void ExtensionContextMenuModelTest::RefreshMenu(
    ExtensionContextMenuModel* model) {
  model->InitMenu(model->GetExtension());
}

int ExtensionContextMenuModelTest::CountExtensionItems(
    ExtensionContextMenuModel* model) {
  return model->extension_items_count_;
}

namespace {

// Tests that applicable menu items are disabled when a ManagementPolicy
// prohibits them.
TEST_F(ExtensionContextMenuModelTest, PolicyDisablesItems) {
  InitializeEmptyExtensionService();
  scoped_refptr<Extension> extension = BuildExtension();
  ASSERT_TRUE(extension.get());
  service_->AddExtension(extension.get());

  // Create a Browser for the ExtensionContextMenuModel to use.
  Browser::CreateParams params(profile_.get(), chrome::GetActiveDesktop());
  TestBrowserWindow test_window;
  params.window = &test_window;
  Browser browser(params);

  scoped_refptr<ExtensionContextMenuModel> menu(
      new ExtensionContextMenuModel(extension.get(), &browser));

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

TEST_F(ExtensionContextMenuModelTest, ExtensionItemTest) {
  InitializeEmptyExtensionService();
  scoped_refptr<Extension> extension = BuildExtension();
  ASSERT_TRUE(extension.get());
  service_->AddExtension(extension.get());

  // Create a Browser for the ExtensionContextMenuModel to use.
  Browser::CreateParams params(profile_.get(), chrome::GetActiveDesktop());
  TestBrowserWindow test_window;
  params.window = &test_window;
  Browser browser(params);

  // Create a MenuManager for adding context items.
  MenuManager* manager = static_cast<MenuManager*>(
      (MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(), &MenuManagerFactory::BuildServiceInstanceForTesting)));
  ASSERT_TRUE(manager);

  scoped_refptr<ExtensionContextMenuModel> menu(
      new ExtensionContextMenuModel(extension.get(), &browser));

  // There should be no extension items yet.
  EXPECT_EQ(0, CountExtensionItems(menu));

  // Add a browser action menu item for |extension| to |manager|.
  AddContextItemAndRefreshModel(
      manager, extension.get(), MenuItem::BROWSER_ACTION, menu);

  // Since |extension| has a page action, the browser action menu item should
  // not be present.
  EXPECT_EQ(0, CountExtensionItems(menu));

  // Add a page action menu item and reset the context menu.
  AddContextItemAndRefreshModel(
      manager, extension.get(), MenuItem::PAGE_ACTION, menu);

  // The page action item should be present because |extension| has a page
  // action.
  EXPECT_EQ(1, CountExtensionItems(menu));

  // Create more page action items to test top level menu item limitations.
  for (int i = 0; i < api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT; ++i)
    AddContextItemAndRefreshModel(
        manager, extension.get(), MenuItem::PAGE_ACTION, menu);

  // The menu should only have a limited number of extension items, since they
  // are all top level items, and we limit the number of top level extension
  // items.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(menu));

  AddContextItemAndRefreshModel(
      manager, extension.get(), MenuItem::PAGE_ACTION, menu);

  // Adding another top level item should not increase the count.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(menu));
}

}  // namespace
}  // namespace extensions

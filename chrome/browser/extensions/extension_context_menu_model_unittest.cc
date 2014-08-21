// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Build an extension to pass to the menu constructor, with the an action
// specified by |action_key|.
scoped_refptr<const Extension> BuildExtension(const std::string& name,
                                              const char* action_key) {
  return ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                       .Set("name", name)
                       .Set("version", "1")
                       .Set("manifest_version", 2)
                       .Set(action_key, DictionaryBuilder().Pass()))
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

// Create a Browser for the ExtensionContextMenuModel to use.
scoped_ptr<Browser> CreateBrowser(Profile* profile) {
  Browser::CreateParams params(profile, chrome::GetActiveDesktop());
  TestBrowserWindow test_window;
  params.window = &test_window;
  return scoped_ptr<Browser>(new Browser(params));
}

// Returns the index of the given |command_id| in the given |menu|, or -1 if it
// is not found.
int GetCommandIndex(const scoped_refptr<ExtensionContextMenuModel> menu,
                         int command_id) {
  int item_count = menu->GetItemCount();
  for (int i = 0; i < item_count; ++i) {
    if (menu->GetCommandIdAt(i) == command_id)
      return i;
  }
  return -1;
}

}  // namespace

class ExtensionContextMenuModelTest : public ExtensionServiceTestBase {
 public:
  ExtensionContextMenuModelTest();

  // Creates an extension menu item for |extension| with the given |context|
  // and adds it to |manager|. Refreshes |model| to show new item.
  void AddContextItemAndRefreshModel(MenuManager* manager,
                                     const Extension* extension,
                                     MenuItem::Context context,
                                     ExtensionContextMenuModel* model);

  // Reinitializes the given |model|.
  void RefreshMenu(ExtensionContextMenuModel* model);

  // Returns the number of extension menu items that show up in |model|.
  int CountExtensionItems(ExtensionContextMenuModel* model);

 private:
  int cur_id_;
};

ExtensionContextMenuModelTest::ExtensionContextMenuModelTest() : cur_id_(0) {
}


void ExtensionContextMenuModelTest::AddContextItemAndRefreshModel(
    MenuManager* manager,
    const Extension* extension,
    MenuItem::Context context,
    ExtensionContextMenuModel* model) {
  MenuItem::Type type = MenuItem::NORMAL;
  MenuItem::ContextList contexts(context);
  const MenuItem::ExtensionKey key(extension->id());
  MenuItem::Id id(false, key);
  id.uid = ++cur_id_;
  manager->AddContextItem(extension,
                          new MenuItem(id,
                                       "test",
                                       false,  // checked
                                       true,   // enabled
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

// Tests that applicable menu items are disabled when a ManagementPolicy
// prohibits them.
TEST_F(ExtensionContextMenuModelTest, PolicyDisablesItems) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension =
      BuildExtension("extension", manifest_keys::kPageAction);
  ASSERT_TRUE(extension.get());
  service()->AddExtension(extension.get());

  scoped_ptr<Browser> browser = CreateBrowser(profile());

  scoped_refptr<ExtensionContextMenuModel> menu(
      new ExtensionContextMenuModel(extension.get(), browser.get()));

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  system->management_policy()->UnregisterAllProviders();

  // Actions should be enabled.
  ASSERT_TRUE(menu->IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));

  TestManagementPolicyProvider policy_provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  system->management_policy()->RegisterProvider(&policy_provider);

  // Now the actions are disabled.
  ASSERT_FALSE(menu->IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));

  // Don't leave |policy_provider| dangling.
  system->management_policy()->UnregisterAllProviders();
}

TEST_F(ExtensionContextMenuModelTest, ExtensionItemTest) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension =
      BuildExtension("extension", manifest_keys::kPageAction);
  ASSERT_TRUE(extension.get());
  service()->AddExtension(extension.get());

  scoped_ptr<Browser> browser = CreateBrowser(profile());

  // Create a MenuManager for adding context items.
  MenuManager* manager = static_cast<MenuManager*>(
      (MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(),
          &MenuManagerFactory::BuildServiceInstanceForTesting)));
  ASSERT_TRUE(manager);

  scoped_refptr<ExtensionContextMenuModel> menu(
      new ExtensionContextMenuModel(extension.get(), browser.get()));

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

// Test that the "show" and "hide" menu items appear correctly in the extension
// context menu.
TEST_F(ExtensionContextMenuModelTest, ExtensionContextMenuShowAndHide) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> page_action =
      BuildExtension("page_action_extension", manifest_keys::kPageAction);
  ASSERT_TRUE(page_action.get());
  scoped_refptr<const Extension> browser_action =
      BuildExtension("browser_action_extension", manifest_keys::kBrowserAction);
  ASSERT_TRUE(browser_action.get());

  service()->AddExtension(page_action.get());
  service()->AddExtension(browser_action.get());

  scoped_ptr<Browser> browser = CreateBrowser(profile());

  scoped_refptr<ExtensionContextMenuModel> menu(
      new ExtensionContextMenuModel(page_action.get(), browser.get()));

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries visibility_command =
      ExtensionContextMenuModel::TOGGLE_VISIBILITY;
  base::string16 hide_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_BUTTON);
  base::string16 show_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  int index = GetCommandIndex(menu, visibility_command);
  // Without the toolbar redesign switch, page action menus shouldn't have a
  // visibility option.
  EXPECT_EQ(-1, index);

  menu = new ExtensionContextMenuModel(browser_action.get(), browser.get());
  index = GetCommandIndex(menu, visibility_command);
  // Browser actions should have the visibility option.
  EXPECT_NE(-1, index);

  // Enabling the toolbar redesign switch should give page actions the button.
  FeatureSwitch::ScopedOverride enable_toolbar_redesign(
      FeatureSwitch::extension_action_redesign(), true);
  menu = new ExtensionContextMenuModel(page_action.get(), browser.get());
  index = GetCommandIndex(menu, visibility_command);
  EXPECT_NE(-1, index);

  // Next, we test the command label.
  menu = new ExtensionContextMenuModel(browser_action.get(), browser.get());
  index = GetCommandIndex(menu, visibility_command);
  // By default, browser actions should be visible (and therefore the button
  // should be to hide).
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, browser_action->id()));
  EXPECT_EQ(hide_string, menu->GetLabelAt(index));

  // Hide the browser action. This should mean the string is "show".
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, browser_action->id(), false);
  menu = new ExtensionContextMenuModel(browser_action.get(), browser.get());
  index = GetCommandIndex(menu, visibility_command);
  EXPECT_NE(-1, index);
  EXPECT_EQ(show_string, menu->GetLabelAt(index));
}

}  // namespace extensions

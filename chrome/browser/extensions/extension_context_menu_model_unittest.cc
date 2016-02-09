// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace extensions {

namespace {

void Increment(int* i) {
  CHECK(i);
  ++(*i);
}

// Label for test extension menu item.
const char* kTestExtensionItemLabel = "test-ext-item";

class MenuBuilder {
 public:
  MenuBuilder(scoped_refptr<const Extension> extension,
              Browser* browser,
              MenuManager* menu_manager)
      : extension_(extension),
        browser_(browser),
        menu_manager_(menu_manager),
        cur_id_(0) {}
  ~MenuBuilder() {}

  scoped_ptr<ExtensionContextMenuModel> BuildMenu() {
    return make_scoped_ptr(new ExtensionContextMenuModel(
        extension_.get(), browser_, ExtensionContextMenuModel::VISIBLE,
        nullptr));
  }

  void AddContextItem(MenuItem::Context context) {
    MenuItem::Id id(false /* not incognito */,
                    MenuItem::ExtensionKey(extension_->id()));
    id.uid = ++cur_id_;
    menu_manager_->AddContextItem(
        extension_.get(),
        new MenuItem(id, kTestExtensionItemLabel,
                     false,  // check`ed
                     true,   // enabled
                     MenuItem::NORMAL, MenuItem::ContextList(context)));
  }

 private:
  scoped_refptr<const Extension> extension_;
  Browser* browser_;
  MenuManager* menu_manager_;
  int cur_id_;

  DISALLOW_COPY_AND_ASSIGN(MenuBuilder);
};

// Returns the number of extension menu items that show up in |model|.
// For this test, all the extension items have same label
// |kTestExtensionItemLabel|.
int CountExtensionItems(const ExtensionContextMenuModel& model) {
  base::string16 expected_label = base::ASCIIToUTF16(kTestExtensionItemLabel);
  int num_items_found = 0;
  int num_custom_found = 0;
  for (int i = 0; i < model.GetItemCount(); ++i) {
    if (expected_label == model.GetLabelAt(i))
      ++num_items_found;
    int command_id = model.GetCommandIdAt(i);
    if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
        command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST)
      ++num_custom_found;
  }
  // The only custom extension items present on the menu should be those we
  // added in the test.
  EXPECT_EQ(num_items_found, num_custom_found);
  return num_items_found;
}

}  // namespace

class ExtensionContextMenuModelTest : public ExtensionServiceTestBase {
 public:
  ExtensionContextMenuModelTest();

  // Build an extension to pass to the menu constructor, with the action
  // specified by |action_key|.
  const Extension* AddExtension(const std::string& name,
                                const char* action_key,
                                Manifest::Location location);
  const Extension* AddExtensionWithHostPermission(
      const std::string& name,
      const char* action_key,
      Manifest::Location location,
      const std::string& host_permission);

  Browser* GetBrowser();

 private:
  scoped_ptr<TestBrowserWindow> test_window_;
  scoped_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContextMenuModelTest);
};

ExtensionContextMenuModelTest::ExtensionContextMenuModelTest() {}

const Extension* ExtensionContextMenuModelTest::AddExtension(
    const std::string& name,
    const char* action_key,
    Manifest::Location location) {
  return AddExtensionWithHostPermission(name, action_key, location,
                                        std::string());
}

const Extension* ExtensionContextMenuModelTest::AddExtensionWithHostPermission(
    const std::string& name,
    const char* action_key,
    Manifest::Location location,
    const std::string& host_permission) {
  DictionaryBuilder manifest;
  manifest.Set("name", name)
      .Set("version", "1")
      .Set("manifest_version", 2)
      .Set(action_key, DictionaryBuilder());
  if (!host_permission.empty())
    manifest.Set("permissions",
                 std::move(ListBuilder().Append(host_permission)));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetID(crx_file::id_util::GenerateId(name))
          .SetLocation(location)
          .Build();
  if (!extension.get())
    ADD_FAILURE();
  service()->AddExtension(extension.get());
  return extension.get();
}

Browser* ExtensionContextMenuModelTest::GetBrowser() {
  if (!browser_) {
    Browser::CreateParams params(profile());
    test_window_.reset(new TestBrowserWindow());
    params.window = test_window_.get();
    browser_.reset(new Browser(params));
  }
  return browser_.get();
}

// Tests that applicable menu items are disabled when a ManagementPolicy
// prohibits them.
TEST_F(ExtensionContextMenuModelTest, RequiredInstallationsDisablesItems) {
  InitializeEmptyExtensionService();

  // Test that management policy can determine whether or not policy-installed
  // extensions can be installed/uninstalled.
  const Extension* extension = AddExtension(
      "extension", manifest_keys::kPageAction, Manifest::EXTERNAL_POLICY);

  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr);

  ExtensionSystem* system = ExtensionSystem::Get(profile());
  system->management_policy()->UnregisterAllProviders();

  // Uninstallation should be, by default, enabled.
  EXPECT_TRUE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));

  TestManagementPolicyProvider policy_provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  system->management_policy()->RegisterProvider(&policy_provider);

  // If there's a policy provider that requires the extension stay enabled, then
  // uninstallation should be disabled.
  EXPECT_FALSE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::UNINSTALL));
  int uninstall_index =
      menu.GetIndexOfCommandId(ExtensionContextMenuModel::UNINSTALL);
  // There should also be an icon to visually indicate why uninstallation is
  // forbidden.
  gfx::Image icon;
  EXPECT_TRUE(menu.GetIconAt(uninstall_index, &icon));
  EXPECT_FALSE(icon.IsEmpty());

  // Don't leave |policy_provider| dangling.
  system->management_policy()->UnregisterProvider(&policy_provider);
}

// Tests the context menu for a component extension.
TEST_F(ExtensionContextMenuModelTest, ComponentExtensionContextMenu) {
  InitializeEmptyExtensionService();

  std::string name("component");
  scoped_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", name)
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("browser_action", DictionaryBuilder())
          .Build();

  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(make_scoped_ptr(manifest->DeepCopy()))
            .SetID(crx_file::id_util::GenerateId("component"))
            .SetLocation(Manifest::COMPONENT)
            .Build();
    service()->AddExtension(extension.get());

    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr);

    // A component extension's context menu should not include options for
    // managing extensions or removing it, and should only include an option for
    // the options page if the extension has one (which this one doesn't).
    EXPECT_EQ(-1,
              menu.GetIndexOfCommandId(ExtensionContextMenuModel::CONFIGURE));
    EXPECT_EQ(-1,
              menu.GetIndexOfCommandId(ExtensionContextMenuModel::UNINSTALL));
    EXPECT_EQ(-1, menu.GetIndexOfCommandId(ExtensionContextMenuModel::MANAGE));
    // The "name" option should be present, but not enabled for component
    // extensions.
    EXPECT_NE(-1, menu.GetIndexOfCommandId(ExtensionContextMenuModel::NAME));
    EXPECT_FALSE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::NAME));
  }

  {
    // Check that a component extension with an options page does have the
    // options
    // menu item, and it is enabled.
    manifest->SetString("options_page", "options_page.html");
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .SetID(crx_file::id_util::GenerateId("component_opts"))
            .SetLocation(Manifest::COMPONENT)
            .Build();
    ExtensionContextMenuModel menu(extension.get(), GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr);
    service()->AddExtension(extension.get());
    EXPECT_TRUE(extensions::OptionsPageInfo::HasOptionsPage(extension.get()));
    EXPECT_NE(-1,
              menu.GetIndexOfCommandId(ExtensionContextMenuModel::CONFIGURE));
    EXPECT_TRUE(menu.IsCommandIdEnabled(ExtensionContextMenuModel::CONFIGURE));
  }
}

TEST_F(ExtensionContextMenuModelTest, ExtensionItemTest) {
  InitializeEmptyExtensionService();
  const Extension* extension =
      AddExtension("extension", manifest_keys::kPageAction, Manifest::INTERNAL);

  // Create a MenuManager for adding context items.
  MenuManager* manager = static_cast<MenuManager*>(
      (MenuManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(),
          &MenuManagerFactory::BuildServiceInstanceForTesting)));
  ASSERT_TRUE(manager);

  MenuBuilder builder(extension, GetBrowser(), manager);

  // There should be no extension items yet.
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  // Add a browser action menu item.
  builder.AddContextItem(MenuItem::BROWSER_ACTION);
  // Since |extension| has a page action, the browser action menu item should
  // not be present.
  EXPECT_EQ(0, CountExtensionItems(*builder.BuildMenu()));

  // Add a page action menu item. This should be present because |extension|
  // has a page action.
  builder.AddContextItem(MenuItem::PAGE_ACTION);
  EXPECT_EQ(1, CountExtensionItems(*builder.BuildMenu()));

  // Create more page action items to test top level menu item limitations.
  // We start at 1, so this should try to add the limit + 1.
  for (int i = 0; i < api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT; ++i)
    builder.AddContextItem(MenuItem::PAGE_ACTION);

  // We shouldn't go above the limit of top-level items.
  EXPECT_EQ(api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT,
            CountExtensionItems(*builder.BuildMenu()));
}

// Test that the "show" and "hide" menu items appear correctly in the extension
// context menu without the toolbar redesign.
TEST_F(ExtensionContextMenuModelTest, ExtensionContextMenuShowAndHideLegacy) {
  // Start with the toolbar redesign disabled.
  scoped_ptr<FeatureSwitch::ScopedOverride> toolbar_redesign_override(
      new FeatureSwitch::ScopedOverride(
          FeatureSwitch::extension_action_redesign(), false));

  InitializeEmptyExtensionService();
  Browser* browser = GetBrowser();
  extension_action_test_util::CreateToolbarModelForProfile(profile());

  const Extension* page_action = AddExtension(
      "page_action_extension", manifest_keys::kPageAction, Manifest::INTERNAL);
  const Extension* browser_action =
      AddExtension("browser_action_extension", manifest_keys::kBrowserAction,
                   Manifest::INTERNAL);

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries visibility_command =
      ExtensionContextMenuModel::TOGGLE_VISIBILITY;
  base::string16 hide_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_BUTTON);

  {
    ExtensionContextMenuModel menu(page_action, browser,
                                   ExtensionContextMenuModel::VISIBLE, nullptr);

    // Without the toolbar redesign switch, page action menus shouldn't have a
    // visibility option.
    EXPECT_EQ(-1, menu.GetIndexOfCommandId(visibility_command));
  }

  {
    ExtensionContextMenuModel menu(browser_action, browser,
                                   ExtensionContextMenuModel::VISIBLE, nullptr);
    int index = menu.GetIndexOfCommandId(visibility_command);
    // Browser actions should have the visibility option.
    EXPECT_NE(-1, index);
    // Since the action is currently visible, it should have the option to hide
    // it.
    EXPECT_EQ(hide_string, menu.GetLabelAt(index));
  }

  {
    ExtensionContextMenuModel menu(browser_action, browser,
                                   ExtensionContextMenuModel::OVERFLOWED,
                                   nullptr);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    // Without the redesign, 'hiding' refers to removing the action from the
    // toolbar entirely, so even with the action overflowed, the string should
    // be 'Hide action'.
    EXPECT_EQ(hide_string, menu.GetLabelAt(index));

    ExtensionActionAPI* action_api = ExtensionActionAPI::Get(profile());
    // At the start, the action should be visible.
    EXPECT_TRUE(action_api->GetBrowserActionVisibility(browser_action->id()));
    menu.ExecuteCommand(visibility_command, 0);
    // After execution, it should be hidden.
    EXPECT_FALSE(action_api->GetBrowserActionVisibility(browser_action->id()));

    // Cleanup - make the action visible again.
    action_api->SetBrowserActionVisibility(browser_action->id(), true);
  }
}

// Test that the "show" and "hide" menu items appear correctly in the extension
// context menu with the toolbar redesign.
TEST_F(ExtensionContextMenuModelTest, ExtensionContextMenuShowAndHideRedesign) {
  // Start with the toolbar redesign disabled.
  scoped_ptr<FeatureSwitch::ScopedOverride> toolbar_redesign_override(
      new FeatureSwitch::ScopedOverride(
          FeatureSwitch::extension_action_redesign(), true));

  InitializeEmptyExtensionService();
  Browser* browser = GetBrowser();
  extension_action_test_util::CreateToolbarModelForProfile(profile());
  const Extension* page_action =
      AddExtension("page_action_extension",
                     manifest_keys::kPageAction,
                     Manifest::INTERNAL);
  const Extension* browser_action =
      AddExtension("browser_action_extension",
                     manifest_keys::kBrowserAction,
                     Manifest::INTERNAL);

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries visibility_command =
      ExtensionContextMenuModel::TOGGLE_VISIBILITY;
  base::string16 redesign_hide_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_HIDE_BUTTON_IN_MENU);
  base::string16 redesign_show_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_BUTTON_IN_TOOLBAR);
  base::string16 redesign_keep_string =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_KEEP_BUTTON_IN_TOOLBAR);

  {
    // Even page actions should have a visibility option with the redesign on.
    ExtensionContextMenuModel menu(page_action, browser,
                                   ExtensionContextMenuModel::VISIBLE, nullptr);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(redesign_hide_string, menu.GetLabelAt(index));
  }

  {
    ExtensionContextMenuModel menu(browser_action, browser,
                                   ExtensionContextMenuModel::VISIBLE, nullptr);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(redesign_hide_string, menu.GetLabelAt(index));

    ExtensionActionAPI* action_api = ExtensionActionAPI::Get(profile());
    EXPECT_TRUE(action_api->GetBrowserActionVisibility(browser_action->id()));
    // Executing the 'hide' command shouldn't modify the prefs with the redesign
    // turned on (the ordering behavior is tested in ToolbarActionsModel tests).
    menu.ExecuteCommand(visibility_command, 0);
    EXPECT_TRUE(action_api->GetBrowserActionVisibility(browser_action->id()));
  }

  {
    // If the action is overflowed, it should have the "Show button in toolbar"
    // string.
    ExtensionContextMenuModel menu(browser_action, browser,
                                   ExtensionContextMenuModel::OVERFLOWED,
                                   nullptr);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(redesign_show_string, menu.GetLabelAt(index));
  }

  {
    // If the action is transitively visible, as happens when it is showing a
    // popup, we should use a "Keep button in toolbar" string.
    ExtensionContextMenuModel menu(
        browser_action, browser,
        ExtensionContextMenuModel::TRANSITIVELY_VISIBLE, nullptr);
    int index = menu.GetIndexOfCommandId(visibility_command);
    EXPECT_NE(-1, index);
    EXPECT_EQ(redesign_keep_string, menu.GetLabelAt(index));
  }
}

TEST_F(ExtensionContextMenuModelTest, ExtensionContextUninstall) {
  InitializeEmptyExtensionService();

  const Extension* extension = AddExtension(
      "extension", manifest_keys::kBrowserAction, Manifest::INTERNAL);
  const std::string extension_id = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(extension_id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  TestExtensionRegistryObserver uninstalled_observer(registry());
  {
    // Scope the menu so that it's destroyed during the uninstall process. This
    // reflects what normally happens (Chrome closes the menu when the uninstall
    // dialog shows up).
    ExtensionContextMenuModel menu(extension, GetBrowser(),
                                   ExtensionContextMenuModel::VISIBLE, nullptr);
    menu.ExecuteCommand(ExtensionContextMenuModel::UNINSTALL, 0);
  }
  uninstalled_observer.WaitForExtensionUninstalled();
  EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                            ExtensionRegistry::EVERYTHING));
}

TEST_F(ExtensionContextMenuModelTest, TestPageAccessSubmenu) {
  // This test relies on the click-to-script feature.
  scoped_ptr<FeatureSwitch::ScopedOverride> enable_scripts_require_action(
      new FeatureSwitch::ScopedOverride(FeatureSwitch::scripts_require_action(),
                                        true));
  InitializeEmptyExtensionService();

  // Add an extension with all urls.
  const Extension* extension =
      AddExtensionWithHostPermission("extension", manifest_keys::kBrowserAction,
                                     Manifest::INTERNAL, "*://*/*");

  const GURL kActiveUrl("http://www.example.com/");
  const GURL kOtherUrl("http://www.google.com/");

  // Add a web contents to the browser.
  content::TestWebContentsFactory factory;
  content::WebContents* contents = factory.CreateWebContents(profile());
  Browser* browser = GetBrowser();
  browser->tab_strip_model()->AppendWebContents(contents, true);
  EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents(), contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(contents);
  web_contents_tester->NavigateAndCommit(kActiveUrl);

  ActiveScriptController* active_script_controller =
      ActiveScriptController::GetForWebContents(contents);
  ASSERT_TRUE(active_script_controller);

  // Pretend the extension wants to run.
  int run_count = 0;
  base::Closure increment_run_count(base::Bind(&Increment, &run_count));
  active_script_controller->RequestScriptInjectionForTesting(
      extension, UserScript::DOCUMENT_IDLE, increment_run_count);

  ExtensionContextMenuModel menu(extension, GetBrowser(),
                                 ExtensionContextMenuModel::VISIBLE, nullptr);

  EXPECT_NE(-1, menu.GetIndexOfCommandId(
                    ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU));

  // For laziness.
  const ExtensionContextMenuModel::MenuEntries kRunOnClick =
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK;
  const ExtensionContextMenuModel::MenuEntries kRunOnSite =
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE;
  const ExtensionContextMenuModel::MenuEntries kRunOnAllSites =
      ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES;

  // Initial state: The extension should be in "run on click" mode.
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // Initial state: The extension should have all permissions withheld, so
  // shouldn't be allowed to run on the active url or another arbitrary url, and
  // should have withheld permissions.
  ScriptingPermissionsModifier permissions_modifier(profile(), extension);
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  const PermissionsData* permissions = extension->permissions_data();
  EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

  // Change the mode to be "Run on site".
  menu.ExecuteCommand(kRunOnSite, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // The extension should have access to the active url, but not to another
  // arbitrary url, and the extension should still have withheld permissions.
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

  // Since the extension has permission, it should have ran.
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(active_script_controller->WantsToRun(extension));

  // On another url, the mode should still be run on click.
  web_contents_tester->NavigateAndCommit(kOtherUrl);
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // And returning to the first url should return the mode to run on site.
  web_contents_tester->NavigateAndCommit(kActiveUrl);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // Request another run.
  active_script_controller->RequestScriptInjectionForTesting(
      extension, UserScript::DOCUMENT_IDLE, increment_run_count);

  // Change the mode to be "Run on all sites".
  menu.ExecuteCommand(kRunOnAllSites, 0);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnAllSites));

  // The extension should be able to run on any url, and shouldn't have any
  // withheld permissions.
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_TRUE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  EXPECT_TRUE(permissions->withheld_permissions().IsEmpty());

  // It should have ran again.
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(active_script_controller->WantsToRun(extension));

  // On another url, the mode should also be run on all sites.
  web_contents_tester->NavigateAndCommit(kOtherUrl);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnAllSites));

  web_contents_tester->NavigateAndCommit(kActiveUrl);
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnAllSites));

  active_script_controller->RequestScriptInjectionForTesting(
      extension, UserScript::DOCUMENT_IDLE, increment_run_count);

  // Return the mode to "Run on click".
  menu.ExecuteCommand(kRunOnClick, 0);
  EXPECT_TRUE(menu.IsCommandIdChecked(kRunOnClick));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnSite));
  EXPECT_FALSE(menu.IsCommandIdChecked(kRunOnAllSites));

  // We should return to the initial state - no access.
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kActiveUrl));
  EXPECT_FALSE(permissions_modifier.HasGrantedHostPermission(kOtherUrl));
  EXPECT_FALSE(permissions->withheld_permissions().IsEmpty());

  // And the extension shouldn't have ran.
  EXPECT_EQ(2, run_count);
  EXPECT_TRUE(active_script_controller->WantsToRun(extension));

  // Install an extension requesting only a single host. Since the extension
  // doesn't request all hosts, it shouldn't have withheld permissions, and
  // thus shouldn't have the page access submenu.
  const Extension* single_host_extension = AddExtensionWithHostPermission(
      "single_host_extension", manifest_keys::kBrowserAction,
      Manifest::INTERNAL, "http://www.google.com/*");
  ExtensionContextMenuModel single_host_menu(
      single_host_extension, GetBrowser(), ExtensionContextMenuModel::VISIBLE,
      nullptr);
  EXPECT_EQ(-1, single_host_menu.GetIndexOfCommandId(
                    ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU));

  // Disable the click-to-script feature, and install a new extension requiring
  // all hosts. Since the feature isn't on, it shouldn't have the page access
  // submenu either.
  enable_scripts_require_action.reset();
  enable_scripts_require_action.reset(
      new FeatureSwitch::ScopedOverride(FeatureSwitch::scripts_require_action(),
                                        false));
  const Extension* feature_disabled_extension = AddExtensionWithHostPermission(
      "feature_disabled_extension", manifest_keys::kBrowserAction,
      Manifest::INTERNAL, "http://www.google.com/*");
  ExtensionContextMenuModel feature_disabled_menu(
      feature_disabled_extension, GetBrowser(),
      ExtensionContextMenuModel::VISIBLE, nullptr);
  EXPECT_EQ(-1, feature_disabled_menu.GetIndexOfCommandId(
                    ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU));
}

}  // namespace extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/launcher/launcher_model.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"

using extensions::Extension;

namespace {
const int kExpectedAppIndex = 1;
const char* gmail_app_id = "pjkljhegncpnkpknbcohdijeoejaedia";
const char* gmail_url = "https://mail.google.com/mail/u";
}

class ChromeLauncherControllerPerAppTest : public BrowserWithTestWindowTest {
 protected:
  ChromeLauncherControllerPerAppTest() : extension_service_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    DictionaryValue manifest;
    manifest.SetString("name", "launcher controller test extension");
    manifest.SetString("version", "1");
    manifest.SetString("description", "for testing pinned apps");

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), FilePath(), false);

    std::string error;
    extension1_ = Extension::Create(FilePath(), Extension::LOAD, manifest,
                                    Extension::NO_FLAGS,
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                    &error);
    extension2_ = Extension::Create(FilePath(), Extension::LOAD, manifest,
                                    Extension::NO_FLAGS,
                                    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                                    &error);
    // Fake gmail extension.
    extension3_ = Extension::Create(FilePath(), Extension::LOAD, manifest,
                                    Extension::NO_FLAGS,
                                    gmail_app_id,
                                    &error);

    // Fake search extension.
    extension4_ = Extension::Create(FilePath(), Extension::LOAD, manifest,
                                    Extension::NO_FLAGS,
                                    "coobgpohoikkiipiblmjeljniedjpjpf",
                                    &error);
  }

  void InsertPrefValue(base::ListValue* pref_value,
                       int index,
                       const std::string& extension_id) {
    base::DictionaryValue* entry = new DictionaryValue();
    entry->SetString(ash::kPinnedAppsPrefAppIDPath, extension_id);
    pref_value->Insert(index, entry);
  }

  // Gets the currently configured app launchers from the controller.
  void GetAppLaunchers(ChromeLauncherControllerPerApp* controller,
                       std::vector<std::string>* launchers) {
    launchers->clear();
    for (ash::LauncherItems::const_iterator iter(model_.items().begin());
         iter != model_.items().end(); ++iter) {
      ChromeLauncherControllerPerApp::IDToItemControllerMap::const_iterator
          entry(controller->id_to_item_controller_map_.find(iter->id));
      if (iter->type == ash::TYPE_APP_SHORTCUT &&
          entry != controller->id_to_item_controller_map_.end()) {
        launchers->push_back(entry->second->app_id());
      }
    }
  }

  // Needed for extension service & friends to work.
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  ash::LauncherModel model_;

  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerPerAppTest);
};

TEST_F(ChromeLauncherControllerPerAppTest, DefaultApps) {
  ChromeLauncherControllerPerApp launcher_controller(profile(), &model_);
  launcher_controller.Init();

  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_.item_count());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerPerAppTest, Policy) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     policy_value.DeepCopy());

  // Only |extension1_| should get pinned. |extension2_| is specified but not
  // installed, and |extension3_| is part of the default set, but that shouldn't
  // take effect when the policy override is in place.
  ChromeLauncherControllerPerApp launcher_controller(profile(), &model_);
  launcher_controller.Init();
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ(4, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[kExpectedAppIndex].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[2].type);
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should be reflected in the launcher.
  policy_value.Remove(0, NULL);
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     policy_value.DeepCopy());
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerPerAppTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  ChromeLauncherControllerPerApp launcher_controller(profile(), &model_);
  launcher_controller.Init();

  EXPECT_TRUE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      extension_misc::UNLOAD_REASON_UNINSTALL);

  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));
}

TEST_F(ChromeLauncherControllerPerAppTest, PrefUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());
  ChromeLauncherControllerPerApp controller(profile(), &model_);

  std::vector<std::string> expected_launchers;
  std::vector<std::string> actual_launchers;
  base::ListValue pref_value;
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Unavailable extensions don't create launcher items.
  InsertPrefValue(&pref_value, 0, extension1_->id());
  InsertPrefValue(&pref_value, 1, extension2_->id());
  InsertPrefValue(&pref_value, 2, extension4_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.push_back(extension2_->id());
  expected_launchers.push_back(extension4_->id());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Redundant pref entries show up only once.
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 5, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.insert(expected_launchers.begin() + 1, extension3_->id());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Order changes are reflected correctly.
  pref_value.Clear();
  InsertPrefValue(&pref_value, 0, extension4_->id());
  InsertPrefValue(&pref_value, 1, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension2_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  std::reverse(expected_launchers.begin(), expected_launchers.end());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Clearing works.
  pref_value.Clear();
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.clear();
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

TEST_F(ChromeLauncherControllerPerAppTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());
  ChromeLauncherControllerPerApp controller(profile(), &model_);

  base::ListValue pref_value;
  InsertPrefValue(&pref_value, 0, extension1_->id());
  InsertPrefValue(&pref_value, 1, extension2_->id());
  InsertPrefValue(&pref_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());

  std::vector<std::string> expected_launchers;
  expected_launchers.push_back(extension1_->id());
  expected_launchers.push_back(extension3_->id());
  std::vector<std::string> actual_launchers;

  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Install |extension2| and verify it shows up between the other two.
  extension_service_->AddExtension(extension2_.get());
  expected_launchers.insert(expected_launchers.begin() + 1, extension2_->id());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

// Checks the created menus and menu lists for correctness. It uses the given
// |controller| to create the objects for the given |item| and checks the
// found item count against the |expected_items|. The |title| list contains the
// menu titles in the order of their appearance in the menu (not including the
// application name).
void CheckMenuCreation(ChromeLauncherControllerPerApp* controller,
                       const ash::LauncherItem& item,
                       size_t expected_items,
                       string16 title[]) {
  scoped_ptr<ChromeLauncherAppMenuItems>
      app_list(controller->GetApplicationList(item));
  ChromeLauncherAppMenuItems items(app_list.get());
  // There should be one item in there: The title.
  EXPECT_EQ(expected_items + 1, items.size());
  EXPECT_FALSE(items[0]->IsEnabled());
  for (size_t i = 0; i < expected_items; i++) {
    EXPECT_EQ(title[i], items[1 + i]->title());
  }

  scoped_ptr<ui::MenuModel> menu(controller->CreateApplicationMenu(item));
  // The first element in the menu is a spacing separator. On some systems
  // (e.g. Windows) such things do not exist. As such we check the existence
  // and adjust dynamically.
  int first_item = menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR ? 1 : 0;
  int expected_menu_items = first_item +
                            (expected_items ? (expected_items + 2) : 1);
  EXPECT_EQ(expected_menu_items, menu->GetItemCount());
  EXPECT_FALSE(menu->IsEnabledAt(first_item));
  if (expected_items) {
    EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
              menu->GetTypeAt(first_item + 1));
  }
}

// Check that browsers get reflected correctly in the launcher menu.
TEST_F(ChromeLauncherControllerPerAppTest, BrowserMenuGeneration) {
  EXPECT_EQ(1U, BrowserList::size());
  chrome::NewTab(browser());

  ChromeLauncherControllerPerApp launcher_controller(profile(), &model_);
  launcher_controller.Init();

  // Check that the browser list is empty at this time.
  ash::LauncherItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  CheckMenuCreation(&launcher_controller, item_browser, 0, NULL);

  // Now make the created browser() visible by adding it to the active browser
  // list.
  BrowserList::SetLastActive(browser());
  string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  string16 one_menu_item[] = {title1};
  CheckMenuCreation(&launcher_controller, item_browser, 1, one_menu_item);

  // Create one more browser/window and check that one more was added.
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForProfile(profile()));
  chrome::NewTab(browser2.get());
  BrowserList::SetLastActive(browser2.get());
  string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser2.get(), GURL("http://test2"),
                                      title2);

  // Check that the list contains now two entries - make furthermore sure that
  // the active item is the first entry.
  string16 two_menu_items[] = {title2, title1};
  CheckMenuCreation(&launcher_controller, item_browser, 2, two_menu_items);

  // Apparently we have to close all tabs we have.
  chrome::CloseTab(browser2.get());
}

// Check that V1 apps are correctly reflected in the launcher menu.
TEST_F(ChromeLauncherControllerPerAppTest, V1AppMenuGeneration) {
  EXPECT_EQ(1U, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  BrowserList::SetLastActive(browser());

  ChromeLauncherControllerPerApp launcher_controller(profile(), &model_);
  launcher_controller.Init();

  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_.item_count());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Installing |extension3_| adds it to the launcher.
  ash::LauncherID gmail_id = model_.next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_.item_count());
  int gmail_index = model_.ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension3_->id()));
  launcher_controller.SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));

  // Check the menu content.
  ash::LauncherItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;

  ash::LauncherItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  CheckMenuCreation(&launcher_controller, item_gmail, 0, NULL);

  // Set the gmail URL to a new tab.
  string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);

  string16 one_menu_item[] = {title1};
  CheckMenuCreation(&launcher_controller, item_gmail, 1, one_menu_item);

  // Create one empty tab.
  chrome::NewTab(browser());
  string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(
      browser(),
      GURL("https://bla"),
      title2);

  // and another one with another gmail instance.
  chrome::NewTab(browser());
  string16 title3 = ASCIIToUTF16("Test3");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title3);
  string16 two_menu_items[] = {title3, title1};
  CheckMenuCreation(&launcher_controller, item_gmail, 2, two_menu_items);

  // Even though the item is in the V1 app list, it should also be in the
  // browser list.
  string16 browser_menu_item[] = {title3};
  CheckMenuCreation(&launcher_controller, item_browser, 1, browser_menu_item);

  // Test that closing of (all) the item(s) does work (and all menus get
  // updated properly).
  launcher_controller.Close(item_gmail.id);

  CheckMenuCreation(&launcher_controller, item_gmail, 0, NULL);
  string16 browser_menu_item2[] = {title2};
  CheckMenuCreation(&launcher_controller, item_browser, 1, browser_menu_item2);
}

// Checks that the generated menu list properly activates items.
TEST_F(ChromeLauncherControllerPerAppTest, V1AppMenuExecution) {
  chrome::NewTab(browser());
  BrowserList::SetLastActive(browser());

  ChromeLauncherControllerPerApp launcher_controller(profile(), &model_);
  launcher_controller.Init();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  ash::LauncherID gmail_id = model_.next_id();
  extension_service_->AddExtension(extension3_.get());
  launcher_controller.SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));
  string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);
  chrome::NewTab(browser());
  string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title2);

  // Check that the menu is properly set.
  ash::LauncherItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  string16 two_menu_items[] = {title2, title1};
  CheckMenuCreation(&launcher_controller, item_gmail, 2, two_menu_items);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Execute the first item in the list (which shouldn't do anything since that
  // item is per definition already the active tab).
  {
    scoped_ptr<ui::MenuModel> menu(
        launcher_controller.CreateApplicationMenu(item_gmail));
    // The first element in the menu is a spacing separator. On some systems
    // (e.g. Windows) such things do not exist. As such we check the existence
    // and adjust dynamically.
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 2);
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Execute the second item.
  {
    scoped_ptr<ui::MenuModel> menu(
        launcher_controller.CreateApplicationMenu(item_gmail));
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 3);
  }
  // Now the active tab should be the second item.
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// TODO(skuhne) Add tests for:
//   - V2 apps: create through item in launcher or directly
//   - Tracking correct activation state (seems not to work from unit_test)
//     - Check that browser is always running or active when browser is active
//     - Check that v1 app active shows browser active and app.
//       ..

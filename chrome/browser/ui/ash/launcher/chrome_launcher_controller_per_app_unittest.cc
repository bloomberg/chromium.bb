// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_model_observer.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/shell_window_launcher_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"

using extensions::Extension;
using extensions::Manifest;

namespace {
const int kExpectedAppIndex = 1;
const char* gmail_app_id = "pjkljhegncpnkpknbcohdijeoejaedia";
const char* gmail_url = "https://mail.google.com/mail/u";
}

namespace {

// LauncherModelObserver implementation that tracks what messages are invoked.
class TestLauncherModelObserver : public ash::LauncherModelObserver {
 public:
  TestLauncherModelObserver()
    : added_(0),
      removed_(0),
      changed_(0) {
  }

  virtual ~TestLauncherModelObserver() {
  }

  // LauncherModelObserver
  virtual void LauncherItemAdded(int index) OVERRIDE {
    ++added_;
  }

  virtual void LauncherItemRemoved(int index, ash::LauncherID id) OVERRIDE {
    ++removed_;
  }

  virtual void LauncherItemChanged(int index,
                                   const ash::LauncherItem& old_item) OVERRIDE {
    ++changed_;
  }

  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE {
  }

  virtual void LauncherStatusChanged() OVERRIDE {
  }

  void clear_counts() {
    added_ = 0;
    removed_ = 0;
    changed_ = 0;
  }

  int added() const { return added_; }
  int removed() const { return removed_; }
  int changed() const { return changed_; }

 private:
  int added_;
  int removed_;
  int changed_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherModelObserver);
};

// Test implementation of AppIconLoader.
class TestAppIconLoaderImpl : public extensions::AppIconLoader {
 public:
  TestAppIconLoaderImpl() : fetch_count_(0) {
  }

  virtual ~TestAppIconLoaderImpl() {
  }

  // AppIconLoader implementation:
  virtual void FetchImage(const std::string& id) OVERRIDE {
    ++fetch_count_;
  }

  virtual void ClearImage(const std::string& id) OVERRIDE {
  }

  virtual void UpdateImage(const std::string& id) OVERRIDE {
  }

  int fetch_count() const { return fetch_count_; }

 private:
  int fetch_count_;

  DISALLOW_COPY_AND_ASSIGN(TestAppIconLoaderImpl);
};

}  // namespace

class ChromeLauncherControllerPerAppTest : public BrowserWithTestWindowTest {
 protected:
  ChromeLauncherControllerPerAppTest()
      : BrowserWithTestWindowTest(chrome::HOST_DESKTOP_TYPE_ASH),
        extension_service_(NULL) {
  }

  virtual ~ChromeLauncherControllerPerAppTest() {
  }

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    model_.reset(new ash::LauncherModel);
    model_observer_.reset(new TestLauncherModelObserver);
    model_->AddObserver(model_observer_.get());

    DictionaryValue manifest;
    manifest.SetString("name", "launcher controller test extension");
    manifest.SetString("version", "1");
    manifest.SetString("description", "for testing pinned apps");

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);

    std::string error;
    extension1_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                    &error);
    extension2_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                                    &error);
    // Fake gmail extension.
    extension3_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    gmail_app_id,
                                    &error);

    // Fake search extension.
    extension4_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "coobgpohoikkiipiblmjeljniedjpjpf",
                                    &error);

    // Create a default launcher controller; some tests will call
    // InitLauncherController* to create a new one with a different setup.
    InitLauncherController();
  }

  virtual void TearDown() OVERRIDE {
    model_->RemoveObserver(model_observer_.get());
    model_observer_.reset();
    launcher_controller_.reset();
    model_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  void InitLauncherController() {
    launcher_controller_.reset(
        new ChromeLauncherControllerPerApp(profile(), model_.get()));
    launcher_controller_->Init();
  }

  void InitLauncherControllerWithBrowser() {
    chrome::NewTab(browser());
    BrowserList::SetLastActive(browser());
    InitLauncherController();
  }

  void SetAppIconLoader(extensions::AppIconLoader* loader) {
    launcher_controller_->SetAppIconLoaderForTest(loader);
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
    for (ash::LauncherItems::const_iterator iter(model_->items().begin());
         iter != model_->items().end(); ++iter) {
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
  scoped_ptr<ChromeLauncherControllerPerApp> launcher_controller_;
  scoped_ptr<TestLauncherModelObserver> model_observer_;
  scoped_ptr<ash::LauncherModel> model_;

  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerPerAppTest);
};

TEST_F(ChromeLauncherControllerPerAppTest, DefaultApps) {
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
}

// Check that simple locking of an application will 'create' a launcher item.
TEST_F(ChromeLauncherControllerPerAppTest, CheckLockApps) {
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension2_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension2_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension2_->id()));
}

// Check that multiple locks of an application will be properly handled.
TEST_F(ChromeLauncherControllerPerAppTest, CheckMukltiLockApps) {
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  for (int i = 0; i < 2; i++) {
    launcher_controller_->LockV1AppWithID(extension1_->id());

    EXPECT_EQ(3, model_->item_count());
    EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[kExpectedAppIndex].type);
    EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
    EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(
        extension1_->id()));
  }

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
}

// Check that already pinned items are not effected by locks.
TEST_F(ChromeLauncherControllerPerAppTest, CheckAlreadyPinnedLockApps) {
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppsWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

// Check that already pinned items which get locked stay after unpinning.
TEST_F(ChromeLauncherControllerPerAppTest, CheckPinnedAppsStayAfterUnlock) {
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppsWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

// Check that lock -> pin -> unlock -> unpin does properly transition.
TEST_F(ChromeLauncherControllerPerAppTest, CheckLockPinUnlockUnpin) {
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppsWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
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
  InitLauncherController();
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should be reflected in the launcher.
  policy_value.Remove(0, NULL);
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     policy_value.DeepCopy());
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerPerAppTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  InitLauncherController();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      extension_misc::UNLOAD_REASON_UNINSTALL);

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
}

TEST_F(ChromeLauncherControllerPerAppTest, PrefUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  InitLauncherController();

  std::vector<std::string> expected_launchers;
  std::vector<std::string> actual_launchers;
  base::ListValue pref_value;
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Unavailable extensions don't create launcher items.
  InsertPrefValue(&pref_value, 0, extension1_->id());
  InsertPrefValue(&pref_value, 1, extension2_->id());
  InsertPrefValue(&pref_value, 2, extension4_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.push_back(extension2_->id());
  expected_launchers.push_back(extension4_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Redundant pref entries show up only once.
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 5, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.insert(expected_launchers.begin() + 1, extension3_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Order changes are reflected correctly.
  pref_value.Clear();
  InsertPrefValue(&pref_value, 0, extension4_->id());
  InsertPrefValue(&pref_value, 1, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension2_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  std::reverse(expected_launchers.begin(), expected_launchers.end());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Clearing works.
  pref_value.Clear();
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.clear();
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

TEST_F(ChromeLauncherControllerPerAppTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  InitLauncherController();

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

  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Install |extension2| and verify it shows up between the other two.
  extension_service_->AddExtension(extension2_.get());
  expected_launchers.insert(expected_launchers.begin() + 1, extension2_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

// Checks the created menus and menu lists for correctness. It uses the given
// |controller| to create the objects for the given |item| and checks the
// found item count against the |expected_items|. The |title| list contains the
// menu titles in the order of their appearance in the menu (not including the
// application name).
bool CheckMenuCreation(ChromeLauncherControllerPerApp* controller,
                       const ash::LauncherItem& item,
                       size_t expected_items,
                       string16 title[],
                       bool is_browser) {
  ChromeLauncherAppMenuItems items = controller->GetApplicationList(item, 0);
  // A new behavior has been added: Only show menus if there is at least one
  // item available.
  if (expected_items < 1 && is_browser) {
    EXPECT_EQ(0u, items.size());
    return items.size() == 0;
  }
  // There should be one item in there: The title.
  EXPECT_EQ(expected_items + 1, items.size());
  EXPECT_FALSE(items[0]->IsEnabled());
  for (size_t i = 0; i < expected_items; i++) {
    EXPECT_EQ(title[i], items[1 + i]->title());
    // Check that the first real item has a leading separator.
    if (i == 1)
      EXPECT_TRUE(items[i]->HasLeadingSeparator());
    else
      EXPECT_FALSE(items[i]->HasLeadingSeparator());
  }

  scoped_ptr<ash::LauncherMenuModel> menu(
      controller->CreateApplicationMenu(item, 0));
  // The first element in the menu is a spacing separator. On some systems
  // (e.g. Windows) such things do not exist. As such we check the existence
  // and adjust dynamically.
  int first_item = menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR ? 1 : 0;
  int expected_menu_items = first_item +
                            (expected_items ? (expected_items + 3) : 2);
  EXPECT_EQ(expected_menu_items, menu->GetItemCount());
  EXPECT_FALSE(menu->IsEnabledAt(first_item));
  if (expected_items) {
    EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
              menu->GetTypeAt(first_item + 1));
  }
  return items.size() == expected_items + 1;
}

// Check that browsers get reflected correctly in the launcher menu.
TEST_F(ChromeLauncherControllerPerAppTest, BrowserMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  chrome::NewTab(browser());

  InitLauncherController();

  // Check that the browser list is empty at this time.
  ash::LauncherItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));

  // Now make the created browser() visible by adding it to the active browser
  // list.
  BrowserList::SetLastActive(browser());
  string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  string16 one_menu_item[] = {title1};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item, true));

  // Create one more browser/window and check that one more was added.
  Browser::CreateParams ash_params(profile(), chrome::HOST_DESKTOP_TYPE_ASH);
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&ash_params));
  chrome::NewTab(browser2.get());
  BrowserList::SetLastActive(browser2.get());
  string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser2.get(), GURL("http://test2"),
                                      title2);

  // Check that the list contains now two entries - make furthermore sure that
  // the active item is the first entry.
  string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 2, two_menu_items, true));

  // Apparently we have to close all tabs we have.
  chrome::CloseTab(browser2.get());
}

// Check that V1 apps are correctly reflected in the launcher menu using the
// refocus logic.
// Note that the extension matching logic is tested by the extension system
// and does not need a separate test here.
TEST_F(ChromeLauncherControllerPerAppTest, V1AppMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  InitLauncherControllerWithBrowser();

  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| adds it to the launcher.
  ash::LauncherID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));

  // Check the menu content.
  ash::LauncherItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;

  ash::LauncherItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));

  // Set the gmail URL to a new tab.
  string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);

  string16 one_menu_item[] = {title1};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 1, one_menu_item, false));

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
  string16 two_menu_items[] = {title1, title3};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));

  // Even though the item is in the V1 app list, it should also be in the
  // browser list.
  string16 browser_menu_item[] = {title3};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, browser_menu_item, false));

  // Test that closing of (all) the item(s) does work (and all menus get
  // updated properly).
  launcher_controller_->Close(item_gmail.id);

  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));
  string16 browser_menu_item2[] = {title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, browser_menu_item2, false));
}

// Checks that the generated menu list properly activates items.
TEST_F(ChromeLauncherControllerPerAppTest, V1AppMenuExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  ash::LauncherID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));
  string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);
  chrome::NewTab(browser());
  string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title2);

  // Check that the menu is properly set.
  ash::LauncherItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Execute the second item in the list (which shouldn't do anything since that
  // item is per definition already the active tab).
  {
    scoped_ptr<ash::LauncherMenuModel> menu(
        launcher_controller_->CreateApplicationMenu(item_gmail, 0));
    // The first element in the menu is a spacing separator. On some systems
    // (e.g. Windows) such things do not exist. As such we check the existence
    // and adjust dynamically.
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 3);
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Execute the first item.
  {
    scoped_ptr<ash::LauncherMenuModel> menu(
        launcher_controller_->CreateApplicationMenu(item_gmail, 0));
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 2);
  }
  // Now the active tab should be the second item.
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Checks that the generated menu list properly deletes items.
TEST_F(ChromeLauncherControllerPerAppTest, V1AppMenuDeletionExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  ash::LauncherID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));
  string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);
  chrome::NewTab(browser());
  string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title2);

  // Check that the menu is properly set.
  ash::LauncherItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));

  int tabs = browser()->tab_strip_model()->count();
  // Activate the proper tab through the menu item.
  {
    ChromeLauncherAppMenuItems items =
        launcher_controller_->GetApplicationList(item_gmail, 0);
    items[1]->Execute(0);
    EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  }

  // Delete one tab through the menu item.
  {
    ChromeLauncherAppMenuItems items =
        launcher_controller_->GetApplicationList(item_gmail, 0);
    items[1]->Execute(ui::EF_SHIFT_DOWN);
    EXPECT_EQ(--tabs, browser()->tab_strip_model()->count());
  }
}

// Tests that panels create launcher items correctly
TEST_F(ChromeLauncherControllerPerAppTest, AppPanels) {
  InitLauncherControllerWithBrowser();

  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl();
  SetAppIconLoader(app_icon_loader);

  // Test adding an app panel
  std::string app_id = extension1_->id();
  ShellWindowLauncherItemController app_panel_controller(
      LauncherItemController::TYPE_APP_PANEL, "id", app_id,
      launcher_controller_.get());
  ash::LauncherID launcher_id1 = launcher_controller_->CreateAppLauncherItem(
      &app_panel_controller, app_id, ash::STATUS_RUNNING);
  EXPECT_EQ(1, model_observer_->added());
  EXPECT_EQ(0, model_observer_->changed());
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  model_observer_->clear_counts();

  // App panels should have a separate identifier than the app id
  EXPECT_EQ(0, launcher_controller_->GetLauncherIDForAppID(app_id));

  // Setting the app image image should not change the panel if it set its icon
  app_panel_controller.set_image_set_by_controller(true);
  gfx::ImageSkia image;
  launcher_controller_->SetAppImage(app_id, image);
  EXPECT_EQ(0, model_observer_->changed());

  launcher_controller_->CloseLauncherItem(launcher_id1);
  EXPECT_EQ(1, model_observer_->removed());
}

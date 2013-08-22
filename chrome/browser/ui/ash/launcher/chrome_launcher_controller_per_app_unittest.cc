// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_model_observer.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"

using extensions::Extension;
using extensions::Manifest;

namespace {
const char* offline_gmail_url = "https://mail.google.com/mail/mu/u";
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
    last_index_ = index;
  }

  virtual void LauncherItemRemoved(int index, ash::LauncherID id) OVERRIDE {
    ++removed_;
    last_index_ = index;
  }

  virtual void LauncherItemChanged(int index,
                                   const ash::LauncherItem& old_item) OVERRIDE {
    ++changed_;
    last_index_ = index;
  }

  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE {
    last_index_ = target_index;
  }

  virtual void LauncherStatusChanged() OVERRIDE {
  }

  void clear_counts() {
    added_ = 0;
    removed_ = 0;
    changed_ = 0;
    last_index_ = 0;
  }

  int added() const { return added_; }
  int removed() const { return removed_; }
  int changed() const { return changed_; }
  int last_index() const { return last_index_; }

 private:
  int added_;
  int removed_;
  int changed_;
  int last_index_;

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

// Test implementation of AppTabHelper.
class TestAppTabHelperImpl : public ChromeLauncherController::AppTabHelper {
 public:
  TestAppTabHelperImpl() {}
  virtual ~TestAppTabHelperImpl() {}

  // Sets the id for the specified tab. The id is removed if Remove() is
  // invoked.
  void SetAppID(content::WebContents* tab, const std::string& id) {
    tab_id_map_[tab] = id;
  }

  // Returns true if there is an id registered for |tab|.
  bool HasAppID(content::WebContents* tab) const {
    return tab_id_map_.find(tab) != tab_id_map_.end();
  }

  // AppTabHelper implementation:
  virtual std::string GetAppID(content::WebContents* tab) OVERRIDE {
    return tab_id_map_.find(tab) != tab_id_map_.end() ? tab_id_map_[tab] :
        std::string();
  }

  virtual bool IsValidID(const std::string& id) OVERRIDE {
    for (TabToStringMap::const_iterator i = tab_id_map_.begin();
         i != tab_id_map_.end(); ++i) {
      if (i->second == id)
        return true;
    }
    return false;
  }

 private:
  typedef std::map<content::WebContents*, std::string> TabToStringMap;

  TabToStringMap tab_id_map_;

  DISALLOW_COPY_AND_ASSIGN(TestAppTabHelperImpl);
};

}  // namespace

class ChromeLauncherControllerTest : public BrowserWithTestWindowTest {
 protected:
  ChromeLauncherControllerTest() : extension_service_(NULL) {
    SetHostDesktopType(chrome::HOST_DESKTOP_TYPE_ASH);
  }

  virtual ~ChromeLauncherControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    model_.reset(new ash::LauncherModel);
    model_observer_.reset(new TestLauncherModelObserver);
    model_->AddObserver(model_observer_.get());

    DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName,
                       "launcher controller test extension");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "for testing pinned apps");

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
    DictionaryValue manifest_gmail;
    manifest_gmail.SetString(extensions::manifest_keys::kName,
                             "Gmail launcher controller test extension");
    manifest_gmail.SetString(extensions::manifest_keys::kVersion, "1");
    manifest_gmail.SetString(extensions::manifest_keys::kDescription,
                             "for testing pinned Gmail");
    manifest_gmail.SetString(extensions::manifest_keys::kLaunchWebURL,
                             "https://mail.google.com/mail/ca");
    ListValue* list = new ListValue();
    list->Append(Value::CreateStringValue("*://mail.google.com/mail/ca"));
    manifest_gmail.Set(extensions::manifest_keys::kWebURLs, list);

    extension3_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest_gmail,
                                    Extension::NO_FLAGS,
                                    extension_misc::kGmailAppId,
                                    &error);

    // Fake search extension.
    extension4_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    extension_misc::kGoogleSearchAppId,
                                    &error);
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
        new ChromeLauncherController(profile(), model_.get()));
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

  void SetAppTabHelper(ChromeLauncherController::AppTabHelper* helper) {
    launcher_controller_->SetAppTabHelperForTest(helper);
  }

  void InsertPrefValue(base::ListValue* pref_value,
                       int index,
                       const std::string& extension_id) {
    base::DictionaryValue* entry = new DictionaryValue();
    entry->SetString(ash::kPinnedAppsPrefAppIDPath, extension_id);
    pref_value->Insert(index, entry);
  }

  // Gets the currently configured app launchers from the controller.
  void GetAppLaunchers(ChromeLauncherController* controller,
                       std::vector<std::string>* launchers) {
    launchers->clear();
    for (ash::LauncherItems::const_iterator iter(model_->items().begin());
         iter != model_->items().end(); ++iter) {
      ChromeLauncherController::IDToItemControllerMap::const_iterator
          entry(controller->id_to_item_controller_map_.find(iter->id));
      if (iter->type == ash::TYPE_APP_SHORTCUT &&
          entry != controller->id_to_item_controller_map_.end()) {
        launchers->push_back(entry->second->app_id());
      }
    }
  }

  std::string GetPinnedAppStatus() {
    std::string result;
    for (int i = 0; i < model_->item_count(); i++) {
      switch (model_->items()[i].type) {
        case ash::TYPE_APP_SHORTCUT: {
            const std::string& app =
                launcher_controller_->GetAppIDForLauncherID(
                    model_->items()[i].id);
            if (app == extension1_->id()) {
              result += "App1, ";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
            } else if (app == extension2_->id()) {
              result += "App2, ";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
            } else if (app == extension3_->id()) {
              result += "App3, ";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
            } else if (app == extension4_->id()) {
              result += "App4, ";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
            } else {
              result += "unknown, ";
            }
            break;
          }
        case ash::TYPE_BROWSER_SHORTCUT:
          result += "Chrome, ";
          break;
        case ash::TYPE_APP_LIST:
          result += "AppList, ";
          break;
        default:
          result += "Unknown";
          break;
      }
    }
    return result;
  }

  // Needed for extension service & friends to work.
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  scoped_ptr<ChromeLauncherController> launcher_controller_;
  scoped_ptr<TestLauncherModelObserver> model_observer_;
  scoped_ptr<ash::LauncherModel> model_;

  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerTest);
};

// The testing framework to test the alternate shelf layout.
class AlternateLayoutChromeLauncherControllerTest
    : public ChromeLauncherControllerTest {
 protected:
  AlternateLayoutChromeLauncherControllerTest() {
  }

  virtual ~AlternateLayoutChromeLauncherControllerTest() {
  }

  // Overwrite the Setup function to add the Alternate Shelf layout option.
  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshUseAlternateShelfLayout);
    ChromeLauncherControllerTest::SetUp();
  }

  // Set the index at which the chrome icon should be.
  void SetShelfChromeIconIndex(int index) {
    profile()->GetTestingPrefService()->SetInteger(prefs::kShelfChromeIconIndex,
                                                   index + 1);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(AlternateLayoutChromeLauncherControllerTest);
};


TEST_F(ChromeLauncherControllerTest, DefaultApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Chrome, App3, AppList, ", GetPinnedAppStatus());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsReverseOrder) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(prefs::kShelfChromeIconIndex));
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, AppList, ", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  ash::LauncherItem item;
  extension_service_->AddExtension(extension3_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ("Chrome, App3, AppList, ", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ("Chrome, App2, App3, AppList, ", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2, App3, AppList, ", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrder) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(prefs::kShelfChromeIconIndex));
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, AppList, ", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, App2, AppList, ", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, App1, App2, AppList, ", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Chrome, App1, App2, App3, AppList, ", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrderChromeMoved) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  profile()->GetTestingPrefService()->SetInteger(prefs::kShelfChromeIconIndex,
                                                 1);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, AppList, ", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  ash::LauncherItem item;
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, App2, AppList, ", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("App1, Chrome, App2, AppList, ", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("App1, Chrome, App2, App3, AppList, ", GetPinnedAppStatus());
}

// Check that syncing to a different state does the correct thing.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsResyncOrder) {
  InitLauncherController();
  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(prefs::kShelfChromeIconIndex));
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Chrome, App1, App2, App3, AppList, ", GetPinnedAppStatus());

  // Change the order with increasing chrome position and decreasing position.
  base::ListValue policy_value1;
  InsertPrefValue(&policy_value1, 0, extension3_->id());
  InsertPrefValue(&policy_value1, 1, extension1_->id());
  InsertPrefValue(&policy_value1, 2, extension2_->id());
  profile()->GetTestingPrefService()->SetInteger(prefs::kShelfChromeIconIndex,
                                                 2);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value1.DeepCopy());
  EXPECT_EQ("App3, App1, Chrome, App2, AppList, ", GetPinnedAppStatus());
  base::ListValue policy_value2;
  InsertPrefValue(&policy_value2, 0, extension2_->id());
  InsertPrefValue(&policy_value2, 1, extension3_->id());
  InsertPrefValue(&policy_value2, 2, extension1_->id());
  profile()->GetTestingPrefService()->SetInteger(prefs::kShelfChromeIconIndex,
                                                 1);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value2.DeepCopy());
  EXPECT_EQ("App2, Chrome, App3, App1, AppList, ", GetPinnedAppStatus());
}

TEST_F(AlternateLayoutChromeLauncherControllerTest, DefaultApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App3, ", GetPinnedAppStatus());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_F(AlternateLayoutChromeLauncherControllerTest,
    RestoreDefaultAppsReverseOrder) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  SetShelfChromeIconIndex(0);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, ", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  ash::LauncherItem item;
  extension_service_->AddExtension(extension3_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ("AppList, Chrome, App3, ", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ("AppList, Chrome, App2, App3, ", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3, ", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_F(AlternateLayoutChromeLauncherControllerTest,
    RestoreDefaultAppsRandomOrder) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  SetShelfChromeIconIndex(0);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, ", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App2, ", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App1, App2, ", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3, ", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon - using the alternate shelf layout.
TEST_F(AlternateLayoutChromeLauncherControllerTest,
    RestoreDefaultAppsRandomOrderChromeMoved) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  SetShelfChromeIconIndex(1);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, ", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  ash::LauncherItem item;
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App2, ", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, App1, Chrome, App2, ", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, App1, Chrome, App2, App3, ", GetPinnedAppStatus());
}

// Check that syncing to a different state does the correct thing with the
// alternate shelf layout.
TEST_F(AlternateLayoutChromeLauncherControllerTest,
    RestoreDefaultAppsResyncOrder) {
  InitLauncherController();
  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  // The alternate shelf layout has always one static item at the beginning.
  SetShelfChromeIconIndex(0);
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ("AppList, Chrome, App2, ", GetPinnedAppStatus());
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, ", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3, ", GetPinnedAppStatus());

  // Change the order with increasing chrome position and decreasing position.
  base::ListValue policy_value1;
  InsertPrefValue(&policy_value1, 0, extension3_->id());
  InsertPrefValue(&policy_value1, 1, extension1_->id());
  InsertPrefValue(&policy_value1, 2, extension2_->id());
  SetShelfChromeIconIndex(3);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value1.DeepCopy());
  EXPECT_EQ("AppList, App3, App1, App2, Chrome, ", GetPinnedAppStatus());
  base::ListValue policy_value2;
  InsertPrefValue(&policy_value2, 0, extension2_->id());
  InsertPrefValue(&policy_value2, 1, extension3_->id());
  InsertPrefValue(&policy_value2, 2, extension1_->id());
  SetShelfChromeIconIndex(2);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value2.DeepCopy());
  EXPECT_EQ("AppList, App2, App3, Chrome, App1, ", GetPinnedAppStatus());

  // Check that the chrome icon can also be at the first possible location.
  SetShelfChromeIconIndex(0);
  base::ListValue policy_value3;
  InsertPrefValue(&policy_value3, 0, extension3_->id());
  InsertPrefValue(&policy_value3, 1, extension2_->id());
  InsertPrefValue(&policy_value3, 2, extension1_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value3.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, App2, App1, ", GetPinnedAppStatus());

  // Check that unloading of extensions works as expected.
  extension_service_->UnloadExtension(extension1_->id(),
                                      extension_misc::UNLOAD_REASON_UNINSTALL);
  EXPECT_EQ("AppList, Chrome, App3, App2, ", GetPinnedAppStatus());

  extension_service_->UnloadExtension(extension2_->id(),
                                      extension_misc::UNLOAD_REASON_UNINSTALL);
  EXPECT_EQ("AppList, Chrome, App3, ", GetPinnedAppStatus());

  // Check that an update of an extension does not crash the system.
  extension_service_->UnloadExtension(extension3_->id(),
                                      extension_misc::UNLOAD_REASON_UPDATE);
  EXPECT_EQ("AppList, Chrome, App3, ", GetPinnedAppStatus());
}

// Check that simple locking of an application will 'create' a launcher item.
TEST_F(ChromeLauncherControllerTest, CheckLockApps) {
  InitLauncherController();
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
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[1].type);
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
TEST_F(ChromeLauncherControllerTest, CheckMukltiLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  for (int i = 0; i < 2; i++) {
    launcher_controller_->LockV1AppWithID(extension1_->id());

    EXPECT_EQ(3, model_->item_count());
    EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[1].type);
    EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
    EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(
        extension1_->id()));
  }

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[1].type);
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
TEST_F(ChromeLauncherControllerTest, CheckAlreadyPinnedLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppsWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

// Check that already pinned items which get locked stay after unpinning.
TEST_F(ChromeLauncherControllerTest, CheckPinnedAppsStayAfterUnlock) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppsWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[1].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

// Check that lock -> pin -> unlock -> unpin does properly transition.
TEST_F(ChromeLauncherControllerTest, CheckLockPinUnlockUnpin) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[1].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppsWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

TEST_F(ChromeLauncherControllerTest, Policy) {
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
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should be reflected in the launcher.
  policy_value.Remove(0, NULL);
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     policy_value.DeepCopy());
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerTest, UnpinWithUninstall) {
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

TEST_F(ChromeLauncherControllerTest, PrefUpdates) {
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

TEST_F(ChromeLauncherControllerTest, PendingInsertionOrder) {
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
bool CheckMenuCreation(ChromeLauncherController* controller,
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
TEST_F(ChromeLauncherControllerTest, BrowserMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  chrome::NewTab(browser());

  InitLauncherController();

  // Check that the browser list is empty at this time.
  ash::LauncherItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetLauncherIDForAppID(extension_misc::kChromeAppId);
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
TEST_F(ChromeLauncherControllerTest, V1AppMenuGeneration) {
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
  item_browser.id =
      launcher_controller_->GetLauncherIDForAppID(extension_misc::kChromeAppId);

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
TEST_F(ChromeLauncherControllerTest, V1AppMenuExecution) {
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
TEST_F(ChromeLauncherControllerTest, V1AppMenuDeletionExecution) {
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
TEST_F(ChromeLauncherControllerTest, AppPanels) {
  InitLauncherControllerWithBrowser();
  // Browser shortcut LauncherItem is added.
  EXPECT_EQ(1, model_observer_->added());

  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl();
  SetAppIconLoader(app_icon_loader);

  // Test adding an app panel
  std::string app_id = extension1_->id();
  ShellWindowLauncherItemController app_panel_controller(
      LauncherItemController::TYPE_APP_PANEL, "id", app_id,
      launcher_controller_.get());
  ash::LauncherID launcher_id1 = launcher_controller_->CreateAppLauncherItem(
      &app_panel_controller, app_id, ash::STATUS_RUNNING);
  int panel_index = model_observer_->last_index();
  EXPECT_EQ(2, model_observer_->added());
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
  model_observer_->clear_counts();

  // Add a second app panel and verify that it get the same index as the first
  // one had, being added to the left of the existing panel.
  ash::LauncherID launcher_id2 = launcher_controller_->CreateAppLauncherItem(
      &app_panel_controller, app_id, ash::STATUS_RUNNING);
  EXPECT_EQ(panel_index, model_observer_->last_index());
  EXPECT_EQ(1, model_observer_->added());
  model_observer_->clear_counts();

  launcher_controller_->CloseLauncherItem(launcher_id2);
  launcher_controller_->CloseLauncherItem(launcher_id1);
  EXPECT_EQ(2, model_observer_->removed());
}

// Tests that the Gmail extension matches more then the app itself claims with
// the manifest file.
TEST_F(ChromeLauncherControllerTest, GmailMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check that the launcher controller does not recognize the running app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Installing |extension3_| adds it to the launcher.
  ash::LauncherID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Check that it is now handled.
  EXPECT_TRUE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Check also that the app has detected that properly.
  ash::LauncherItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_EQ(2U, launcher_controller_->GetApplicationList(item_gmail, 0).size());
}

// Tests that the Gmail extension does not match the offline verison.
TEST_F(ChromeLauncherControllerTest, GmailOfflineMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(),
                                      GURL(offline_gmail_url),
                                      title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Installing |extension3_| adds it to the launcher.
  ash::LauncherID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // The content should not be able to be handled by the app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));
}

// Verify that the launcher item positions are persisted and restored.
TEST_F(ChromeLauncherControllerTest, PersistLauncherItemPositions) {
  InitLauncherController();

  TestAppTabHelperImpl* app_tab_helper = new TestAppTabHelperImpl;
  SetAppTabHelper(app_tab_helper);

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[1].type);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip_model->count());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip_model->count());
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");

  EXPECT_FALSE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("1");
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("2");

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[3].type);

  // Move browser shortcut item from index 0 to index 2.
  model_->Move(0, 2);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[3].type);

  launcher_controller_.reset();
  model_.reset(new ash::LauncherModel);
  launcher_controller_.reset(
      ChromeLauncherController::CreateInstance(profile(), model_.get()));
  app_tab_helper = new TestAppTabHelperImpl;
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");
  SetAppTabHelper(app_tab_helper);

  launcher_controller_->Init();

  // Check LauncherItems are restored after resetting ChromeLauncherController.
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[3].type);
}

// Verifies pinned apps are persisted and restored.
TEST_F(ChromeLauncherControllerTest, PersistPinned) {
  InitLauncherControllerWithBrowser();
  size_t initial_size = model_->items().size();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());

  TestAppTabHelperImpl* app_tab_helper = new TestAppTabHelperImpl;
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetAppTabHelper(app_tab_helper);

  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl;
  SetAppIconLoader(app_icon_loader);
  EXPECT_EQ(0, app_icon_loader->fetch_count());

  launcher_controller_->PinAppWithID("1");
  ash::LauncherID id = launcher_controller_->GetLauncherIDForAppID("1");
  int app_index = model_->ItemIndexByID(id);
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[app_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(initial_size + 1, model_->items().size());

  launcher_controller_.reset();
  model_.reset(new ash::LauncherModel);
  launcher_controller_.reset(
      ChromeLauncherController::CreateInstance(profile(), model_.get()));
  app_tab_helper = new TestAppTabHelperImpl;
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetAppTabHelper(app_tab_helper);
  app_icon_loader = new TestAppIconLoaderImpl;
  SetAppIconLoader(app_icon_loader);
  launcher_controller_->Init();

  EXPECT_EQ(1, app_icon_loader->fetch_count());
  ASSERT_EQ(initial_size + 1, model_->items().size());
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[app_index].type);

  launcher_controller_->UnpinAppsWithID("1");
  ASSERT_EQ(initial_size, model_->items().size());
}

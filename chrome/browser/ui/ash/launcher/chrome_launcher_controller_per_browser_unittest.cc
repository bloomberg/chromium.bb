// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_browser.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/launcher/launcher_model.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using extensions::Extension;
using extensions::Manifest;

class ChromeLauncherControllerPerBrowserTest : public testing::Test {
 protected:
  ChromeLauncherControllerPerBrowserTest()
      : profile_(new TestingProfile()),
        extension_service_(NULL) {
    DictionaryValue manifest;
    manifest.SetString("name", "launcher controller test extension");
    manifest.SetString("version", "1");
    manifest.SetString("description", "for testing pinned apps");

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
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
    profile_.reset();
    // Execute any pending deletion tasks.
    base::RunLoop().RunUntilIdle();
  }

  void InsertPrefValue(base::ListValue* pref_value,
                       int index,
                       const std::string& extension_id) {
    base::DictionaryValue* entry = new DictionaryValue();
    entry->SetString(ash::kPinnedAppsPrefAppIDPath, extension_id);
    pref_value->Insert(index, entry);
  }

  // Gets the currently configured app launchers from the controller.
  void GetAppLaunchers(ChromeLauncherControllerPerBrowser* controller,
                       std::vector<std::string>* launchers) {
    launchers->clear();
    for (ash::LauncherItems::const_iterator iter(model_.items().begin());
         iter != model_.items().end(); ++iter) {
      ChromeLauncherControllerPerBrowser::IDToItemControllerMap::const_iterator
          entry(controller->id_to_item_controller_map_.find(iter->id));
      if (iter->type == ash::TYPE_APP_SHORTCUT &&
          entry != controller->id_to_item_controller_map_.end()) {
        launchers->push_back(entry->second->app_id());
      }
    }
  }

  std::string GetPinnedAppStatus(
      ChromeLauncherController* launcher_controller) {
    std::string result;
    for (int i = 0; i < model_.item_count(); i++) {
      switch (model_.items()[i].type) {
        case ash::TYPE_APP_SHORTCUT: {
          const std::string& app =
              launcher_controller->GetAppIDForLauncherID(
                  model_.items()[i].id);
          if (app == extension1_->id()) {
            result += "App1, ";
            EXPECT_TRUE(launcher_controller->IsAppPinned(extension1_->id()));
          } else if (app == extension2_->id()) {
            result += "App2, ";
            EXPECT_TRUE(launcher_controller->IsAppPinned(extension2_->id()));
          } else if (app == extension3_->id()) {
            result += "App3, ";
            EXPECT_TRUE(launcher_controller->IsAppPinned(extension3_->id()));
          } else {
            result += "unknown";
          }
          break;
          }
        case ash::TYPE_BROWSER_SHORTCUT:
          result += "Chrome, ";
          break;
        case ash::TYPE_APP_LIST:
          result += "AppList";
          break;
        default:
          result += "Unknown";
          break;
      }
    }
    return result;
  }

  // Needed for extension service & friends to work.
  content::TestBrowserThreadBundle thread_bundle_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  scoped_ptr<TestingProfile> profile_;
  ash::LauncherModel model_;

  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerPerBrowserTest);
};

TEST_F(ChromeLauncherControllerPerBrowserTest, DefaultApps) {
  ChromeLauncherControllerPerBrowser launcher_controller(profile_.get(),
                                                         &model_);
  launcher_controller.Init();

  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_.item_count());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Chrome, App3, AppList", GetPinnedAppStatus(&launcher_controller));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_F(ChromeLauncherControllerPerBrowserTest, RestoreDefaultAppsReverseOrder) {
  ChromeLauncherControllerPerBrowser launcher_controller(profile_.get(),
                                                         &model_);
  launcher_controller.Init();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 policy_value.DeepCopy());
  EXPECT_EQ(0, profile_->GetPrefs()->GetInteger(prefs::kShelfChromeIconIndex));
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, AppList", GetPinnedAppStatus(&launcher_controller));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  ash::LauncherItem item;
  extension_service_->AddExtension(extension3_.get());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_EQ("Chrome, App3, AppList", GetPinnedAppStatus(&launcher_controller));

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_EQ("Chrome, App2, App3, AppList",
            GetPinnedAppStatus(&launcher_controller));

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2, App3, AppList",
            GetPinnedAppStatus(&launcher_controller));
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_F(ChromeLauncherControllerPerBrowserTest, RestoreDefaultAppsRandomOrder) {
  ChromeLauncherControllerPerBrowser launcher_controller(profile_.get(),
                                                         &model_);
  launcher_controller.Init();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 policy_value.DeepCopy());
  EXPECT_EQ(0, profile_->GetPrefs()->GetInteger(prefs::kShelfChromeIconIndex));
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, AppList", GetPinnedAppStatus(&launcher_controller));

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, App2, AppList", GetPinnedAppStatus(&launcher_controller));

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, App1, App2, AppList",
            GetPinnedAppStatus(&launcher_controller));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Chrome, App1, App2, App3, AppList",
            GetPinnedAppStatus(&launcher_controller));
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon.
TEST_F(ChromeLauncherControllerPerBrowserTest,
    RestoreDefaultAppsRandomOrderChromeMoved) {
  ChromeLauncherControllerPerBrowser launcher_controller(profile_.get(),
                                                         &model_);
  launcher_controller.Init();
  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 policy_value.DeepCopy());
  profile_->GetTestingPrefService()->SetInteger(prefs::kShelfChromeIconIndex,
                                                1);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, AppList", GetPinnedAppStatus(&launcher_controller));

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  ash::LauncherItem item;
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_EQ("Chrome, App2, AppList", GetPinnedAppStatus(&launcher_controller));

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_EQ("App1, Chrome, App2, AppList",
            GetPinnedAppStatus(&launcher_controller));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("App1, Chrome, App2, App3, AppList",
            GetPinnedAppStatus(&launcher_controller));
}

TEST_F(ChromeLauncherControllerPerBrowserTest, Policy) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                    policy_value.DeepCopy());

  // Only |extension1_| should get pinned. |extension2_| is specified but not
  // installed, and |extension3_| is part of the default set, but that shouldn't
  // take effect when the policy override is in place.
  ChromeLauncherControllerPerBrowser launcher_controller(profile_.get(),
                                                         &model_);
  launcher_controller.Init();
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_.items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_.items()[2].type);

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ(4, model_.item_count());
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_.items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_.items()[3].type);
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should be reflected in the launcher.
  policy_value.Remove(0, NULL);
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                    policy_value.DeepCopy());
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerPerBrowserTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  ChromeLauncherControllerPerBrowser launcher_controller(profile_.get(),
                                                         &model_);
  launcher_controller.Init();

  EXPECT_TRUE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      extension_misc::UNLOAD_REASON_UNINSTALL);

  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));
}

TEST_F(ChromeLauncherControllerPerBrowserTest, PrefUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());
  ChromeLauncherControllerPerBrowser controller(profile_.get(), &model_);
  controller.Init();

  std::vector<std::string> expected_launchers;
  std::vector<std::string> actual_launchers;
  base::ListValue pref_value;
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Unavailable extensions don't create launcher items.
  InsertPrefValue(&pref_value, 0, extension1_->id());
  InsertPrefValue(&pref_value, 1, extension2_->id());
  InsertPrefValue(&pref_value, 2, extension4_->id());
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.push_back(extension2_->id());
  expected_launchers.push_back(extension4_->id());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Redundant pref entries show up only once.
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 5, extension3_->id());
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.insert(expected_launchers.begin() + 1, extension3_->id());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Order changes are reflected correctly.
  pref_value.Clear();
  InsertPrefValue(&pref_value, 0, extension4_->id());
  InsertPrefValue(&pref_value, 1, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension2_->id());
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  std::reverse(expected_launchers.begin(), expected_launchers.end());
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Clearing works.
  pref_value.Clear();
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.clear();
  GetAppLaunchers(&controller, &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

TEST_F(ChromeLauncherControllerPerBrowserTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());
  ChromeLauncherControllerPerBrowser controller(profile_.get(), &model_);
  controller.Init();

  base::ListValue pref_value;
  InsertPrefValue(&pref_value, 0, extension1_->id());
  InsertPrefValue(&pref_value, 1, extension2_->id());
  InsertPrefValue(&pref_value, 2, extension3_->id());
  profile_->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
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

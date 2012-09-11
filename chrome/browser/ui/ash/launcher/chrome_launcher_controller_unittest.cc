// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/launcher/launcher_model.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace {
const int kExpectedAppIndex = 2;
}

class ChromeLauncherControllerTest : public testing::Test {
 protected:
  ChromeLauncherControllerTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_),
        profile_(new TestingProfile()),
        extension_service_(NULL) {
    DictionaryValue manifest;
    manifest.SetString("name", "launcher controller test extension");
    manifest.SetString("version", "1");
    manifest.SetString("description", "for testing pinned apps");

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
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
                                    "pjkljhegncpnkpknbcohdijeoejaedia",
                                    &error);
    // Fake search extension.
    extension4_ = Extension::Create(FilePath(), Extension::LOAD, manifest,
                                    Extension::NO_FLAGS,
                                    "coobgpohoikkiipiblmjeljniedjpjpf",
                                    &error);
  }

  virtual void TearDown() OVERRIDE {
    profile_.reset();
    // Execute any pending deletion tasks.
    loop_.RunAllPending();
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
    for (ash::LauncherItems::const_iterator iter(model_.items().begin());
         iter != model_.items().end(); ++iter) {
      ChromeLauncherController::IDToItemMap::const_iterator entry(
          controller->id_to_item_map_.find(iter->id));
      if (iter->type == ash::TYPE_APP_SHORTCUT &&
          entry != controller->id_to_item_map_.end()) {
        launchers->push_back(entry->second.app_id);
      }
    }
  }

  // Needed for extension service & friends to work.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  scoped_ptr<TestingProfile> profile_;
  ash::LauncherModel model_;

  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerTest);
};

TEST_F(ChromeLauncherControllerTest, DefaultApps) {
  ChromeLauncherController launcher_controller(profile_.get(), &model_);
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

TEST_F(ChromeLauncherControllerTest, Policy) {
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
  ChromeLauncherController launcher_controller(profile_.get(), &model_);
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
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                    policy_value.DeepCopy());
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[kExpectedAppIndex].type);
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  ChromeLauncherController launcher_controller(profile_.get(), &model_);
  launcher_controller.Init();

  EXPECT_TRUE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      extension_misc::UNLOAD_REASON_UNINSTALL);

  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));
}

TEST_F(ChromeLauncherControllerTest, PrefUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());
  ChromeLauncherController controller(profile_.get(), &model_);

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

TEST_F(ChromeLauncherControllerTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());
  ChromeLauncherController controller(profile_.get(), &model_);

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

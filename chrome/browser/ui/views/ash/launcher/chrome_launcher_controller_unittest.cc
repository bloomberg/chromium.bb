// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_model.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeLauncherControllerTest : public testing::Test {
 protected:
  ChromeLauncherControllerTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        extension_service_(NULL) {
    DictionaryValue manifest;
    manifest.SetString("name", "launcher controller test extension");
    manifest.SetString("version", "1");
    manifest.SetString("description", "for testing pinned apps");

    TestExtensionSystem* extension_system(
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(&profile_)));
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

  // Needed for extension service & friends to work.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;

  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  TestingProfile profile_;
  ash::LauncherModel model_;

  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerTest);
};

TEST_F(ChromeLauncherControllerTest, DefaultApps) {
  ChromeLauncherController launcher_controller(&profile_, &model_);
  launcher_controller.Init();

  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_.item_count());
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerTest, Policy) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  base::ListValue policy_value;
  base::DictionaryValue* entry1 = new DictionaryValue();
  entry1->SetString(ChromeLauncherController::kPinnedAppsPrefAppIDPath,
                    extension1_->id());
  policy_value.Append(entry1);
  base::DictionaryValue* entry2 = new DictionaryValue();
  entry2->SetString(ChromeLauncherController::kPinnedAppsPrefAppIDPath,
                    extension2_->id());
  policy_value.Append(entry2);
  profile_.GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                   policy_value.DeepCopy());

  // Only |extension1_| should get pinned. |extension2_| is specified but not
  // installed, and |extension3_| is part of the default set, but that shouldn't
  // take effect when the policy override is in place.
  ChromeLauncherController launcher_controller(&profile_, &model_);
  launcher_controller.Init();
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ(4, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[2].type);
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should be reflected in the launcher.
  policy_value.Remove(0, NULL);
  profile_.GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                   policy_value.DeepCopy());
  EXPECT_EQ(3, model_.item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_.items()[1].type);
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerTest, UnpinWithPending) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  ChromeLauncherController launcher_controller(&profile_, &model_);
  launcher_controller.Init();

  EXPECT_TRUE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      extension_misc::UNLOAD_REASON_UNINSTALL);

  EXPECT_FALSE(launcher_controller.IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller.IsAppPinned(extension4_->id()));
}

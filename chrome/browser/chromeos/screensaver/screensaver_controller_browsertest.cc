// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/screensaver/screensaver_controller.h"

#include "ash/screensaver/screensaver_view.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace {

scoped_refptr<extensions::Extension> CreateTestScreensaverExtension() {
  scoped_refptr<extensions::Extension> extension =
      extensions::ExtensionBuilder()
      .SetPath(base::FilePath())
      .SetManifest(extensions::DictionaryBuilder()
                   .Set("name", "Screensaver Extension")
                   .Set("version", "1")
                   .Set("manifest_version", 2)
                   .Set("app", extensions::DictionaryBuilder()
                       .Set("launch", extensions::DictionaryBuilder()
                           .Set("local_path", "")))
                   .Set("permissions", extensions::ListBuilder()
                        .Append("screensaver")))
      .Build();

  return extension;
}

void InstallExtensionToDefaultProfile(const extensions::Extension* extension) {
  Profile* default_profile = ProfileManager::GetDefaultProfile();
  EXPECT_TRUE(default_profile);
  ExtensionService* service =
      extensions::ExtensionSystem::Get(default_profile)->extension_service();
  EXPECT_TRUE(service);

  service->AddExtension(extension);
}

} // namespace

namespace chromeos {

class ScreensaverControllerTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(ScreensaverControllerTest, Basic) {
  scoped_refptr<extensions::Extension> extension(
      CreateTestScreensaverExtension());
  InstallExtensionToDefaultProfile(extension);

  scoped_ptr<ScreensaverController> controller_;
  controller_.reset(new ScreensaverController());
  MessageLoop::current()->RunUntilIdle();

  // Trigger idle.
  controller_->IdleNotify(0);
  EXPECT_TRUE(ash::IsScreensaverShown());

  // Trigger active.
  controller_->OnUserActivity();
  EXPECT_FALSE(ash::IsScreensaverShown());
};

IN_PROC_BROWSER_TEST_F(ScreensaverControllerTest, OutOfOrder) {
  scoped_refptr<extensions::Extension> extension(
      CreateTestScreensaverExtension());
  InstallExtensionToDefaultProfile(extension);

  scoped_ptr<ScreensaverController> controller_;
  controller_.reset(new ScreensaverController());
  MessageLoop::current()->RunUntilIdle();

  // Trigger active.
  controller_->OnUserActivity();
  EXPECT_FALSE(ash::IsScreensaverShown());

  // Trigger idle.
  controller_->IdleNotify(0);
  EXPECT_TRUE(ash::IsScreensaverShown());


  // Trigger idle.
  controller_->IdleNotify(0);
  EXPECT_TRUE(ash::IsScreensaverShown());

  // Trigger active.
  controller_->OnUserActivity();
  EXPECT_FALSE(ash::IsScreensaverShown());
};

}  // namespace chromeos.

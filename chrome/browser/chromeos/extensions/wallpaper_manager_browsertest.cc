// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "grit/browser_resources.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {
const char* kWallpaperManagerExtensionID = "obklkkbkpaoaejdabbfldmcfplpdgolj";
}  // namespace

class WallpaperManagerBrowserTest : public extensions::PlatformAppBrowserTest {
 public:
  WallpaperManagerBrowserTest();
  virtual ~WallpaperManagerBrowserTest();

 protected:
  void VerifyWallpaperManagerLoaded();

 private:
  void LoadAndLaunchWallpaperManager();
};

WallpaperManagerBrowserTest::WallpaperManagerBrowserTest() {
}

WallpaperManagerBrowserTest::~WallpaperManagerBrowserTest() {
}

void WallpaperManagerBrowserTest::LoadAndLaunchWallpaperManager() {
  extension_service()->component_loader()->Add(
      IDR_WALLPAPERMANAGER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("chromeos/wallpaper_manager")));
  const Extension* wallpaper_app =
      ExtensionRegistry::Get(profile())->GetExtensionById(
          kWallpaperManagerExtensionID, ExtensionRegistry::EVERYTHING);
  LaunchPlatformApp(wallpaper_app);
}

void WallpaperManagerBrowserTest::VerifyWallpaperManagerLoaded() {
  ExtensionTestMessageListener window_created_listener(
      "wallpaper-window-created", false);
  ExtensionTestMessageListener launched_listener("launched", false);
  LoadAndLaunchWallpaperManager();
  EXPECT_TRUE(window_created_listener.WaitUntilSatisfied())
      << "Wallpaper picker window was not created.";
  EXPECT_TRUE(launched_listener.WaitUntilSatisfied())
      << "Wallpaper picker was not loaded.";
}

// Test for crbug.com/410550.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, DevLaunchApp) {
  VerifyWallpaperManagerLoaded();
}

// Test for crbug.com/410550. Wallpaper picker should be able to create
// alpha enabled window successfully.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, StableLaunchApp) {
  extensions::ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_STABLE);
  VerifyWallpaperManagerLoaded();
}

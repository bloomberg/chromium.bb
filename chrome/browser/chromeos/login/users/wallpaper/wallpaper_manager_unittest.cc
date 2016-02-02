// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_user_wallpaper_delegate.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using namespace ash;

namespace chromeos {

class WallpaperManagerCacheTest : public test::AshTestBase {
 public:
  WallpaperManagerCacheTest()
      : fake_user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(fake_user_manager_) {}

 protected:
  ~WallpaperManagerCacheTest() override {}

  FakeChromeUserManager* fake_user_manager() { return fake_user_manager_; }

  void SetUp() override {
    test::AshTestBase::SetUp();
    WallpaperManager::Initialize();
  }

  void TearDown() override {
    WallpaperManager::Shutdown();
    test::AshTestBase::TearDown();
  }

  // Creates a test image of size 1x1.
  gfx::ImageSkia CreateTestImage(SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(color);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

 private:
  FakeChromeUserManager* fake_user_manager_;
  ScopedUserManagerEnabler scoped_user_manager_;
};

TEST_F(WallpaperManagerCacheTest, VerifyWallpaperCache) {
  // Add three users to known users.
  const AccountId test_account_id_1 =
      AccountId::FromUserEmail("test1@example.com");
  const AccountId test_account_id_2 =
      AccountId::FromUserEmail("test2@example.com");
  const AccountId test_account_id_3 =
      AccountId::FromUserEmail("test3@example.com");
  base::FilePath path1("path1");
  base::FilePath path2("path2");
  base::FilePath path3("path3");
  fake_user_manager()->AddUser(test_account_id_1);
  fake_user_manager()->AddUser(test_account_id_2);
  fake_user_manager()->AddUser(test_account_id_3);

  // Login two users.
  fake_user_manager()->LoginUser(test_account_id_1);
  fake_user_manager()->LoginUser(test_account_id_2);

  scoped_ptr<WallpaperManager::TestApi> test_api;
  test_api.reset(new WallpaperManager::TestApi(WallpaperManager::Get()));

  gfx::ImageSkia test_user_1_wallpaper = CreateTestImage(SK_ColorRED);
  gfx::ImageSkia test_user_2_wallpaper = CreateTestImage(SK_ColorGREEN);
  gfx::ImageSkia test_user_3_wallpaper = CreateTestImage(SK_ColorWHITE);
  test_api->SetWallpaperCache(test_account_id_1, path1, test_user_1_wallpaper);
  test_api->SetWallpaperCache(test_account_id_2, path2, test_user_2_wallpaper);
  test_api->SetWallpaperCache(test_account_id_3, path3, test_user_3_wallpaper);

  test_api->ClearDisposableWallpaperCache();

  gfx::ImageSkia cached_wallpaper;
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id_1, &cached_wallpaper));
  base::FilePath path;
  EXPECT_TRUE(test_api->GetPathFromCache(test_account_id_1, &path));
  // Logged in users' wallpaper cache should be kept.
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(test_user_1_wallpaper));
  EXPECT_EQ(path, path1);
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id_2, &cached_wallpaper));
  EXPECT_TRUE(test_api->GetPathFromCache(test_account_id_2, &path));
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(test_user_2_wallpaper));
  EXPECT_EQ(path, path2);

  // Not logged in user's wallpaper cache should be cleared.
  EXPECT_FALSE(
      test_api->GetWallpaperFromCache(test_account_id_3, &cached_wallpaper));
  EXPECT_FALSE(test_api->GetPathFromCache(test_account_id_3, &path));
}

// Test that the user's wallpaper cache is cleared after the user is removed.
TEST_F(WallpaperManagerCacheTest, CacheClearedOnUserRemoval) {
  const AccountId test_account_id_1 =
      AccountId::FromUserEmail("test1@example.com");
  const AccountId test_account_id_2 =
      AccountId::FromUserEmail("test2@example.com");
  const base::FilePath path1("user1_custom_path");
  const base::FilePath path2("user2_custom_path");
  fake_user_manager()->AddUser(test_account_id_1);
  fake_user_manager()->AddUser(test_account_id_2);

  scoped_ptr<WallpaperManager::TestApi> test_api(
      new WallpaperManager::TestApi(WallpaperManager::Get()));

  const gfx::ImageSkia test_user_1_wallpaper = CreateTestImage(SK_ColorRED);
  const gfx::ImageSkia test_user_2_wallpaper = CreateTestImage(SK_ColorGREEN);
  test_api->SetWallpaperCache(test_account_id_1, path1, test_user_1_wallpaper);
  test_api->SetWallpaperCache(test_account_id_2, path2, test_user_2_wallpaper);

  gfx::ImageSkia cached_wallpaper;
  // Test that both user1 and user2's wallpaper can be found in cache.
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id_1, &cached_wallpaper));
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id_2, &cached_wallpaper));

  // Remove user2.
  fake_user_manager()->RemoveUserFromList(test_account_id_2);

  // Test that only user1's wallpaper can be found in cache.
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id_1, &cached_wallpaper));
  EXPECT_FALSE(
      test_api->GetWallpaperFromCache(test_account_id_2, &cached_wallpaper));
}

}  // namespace chromeos

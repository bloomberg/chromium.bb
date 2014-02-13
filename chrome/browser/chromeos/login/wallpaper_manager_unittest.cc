// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>

#include "ash/ash_resources/grit/ash_resources.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/chromeos/login/fake_user_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using namespace ash;

const char kTestUser1[] = "test-user@example.com";
const char kTestUser1Hash[] = "test-user@example.com-hash";

namespace chromeos {

class WallpaperManagerTest : public test::AshTestBase {
 public:
  WallpaperManagerTest() : command_line_(CommandLine::NO_PROGRAM) {}

  virtual ~WallpaperManagerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();

    // Register an in-memory local settings instance.
    local_state_.reset(new TestingPrefServiceSimple);
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state_.get());
    UserManager::RegisterPrefs(local_state_->registry());
    // Wallpaper manager and user image managers prefs will be accessed by the
    // unit-test as well.
    UserImageManager::RegisterPrefs(local_state_->registry());
    WallpaperManager::RegisterPrefs(local_state_->registry());

    StartupUtils::RegisterPrefs(local_state_->registry());
    StartupUtils::MarkDeviceRegistered();

    ResetUserManager();
  }

  virtual void TearDown() OVERRIDE {
    // Unregister the in-memory local settings instance.
    TestingBrowserProcess::GetGlobal()->SetLocalState(0);

    // Shut down the DeviceSettingsService.
    DeviceSettingsService::Get()->UnsetSessionManager();

    test::AshTestBase::TearDown();
  }

  void ResetUserManager() {
    // Reset the UserManager singleton.
    user_manager_enabler_.reset();
    // Initialize the UserManager singleton to a fresh UserManagerImpl instance.
    user_manager_enabler_.reset(
        new ScopedUserManagerEnabler(new UserManagerImpl));
  }

  void AppendGuestSwitch() {
    command_line_.AppendSwitch(switches::kGuestSession);
    WallpaperManager::Get()->set_command_line_for_testing(&command_line_);
  }

  void WaitAsyncWallpaperLoad() {
    base::MessageLoop::current()->RunUntilIdle();
  }

 protected:
  CommandLine command_line_;

  scoped_ptr<TestingPrefServiceSimple> local_state_;

  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;

  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperManagerTest);
};

// Test for crbug.com/260755. If this test fails, it is probably because the
// wallpaper of last logged in user is set as guest wallpaper.
TEST_F(WallpaperManagerTest, GuestUserUseGuestWallpaper) {
  UserManager::Get()->UserLoggedIn(kTestUser1, kTestUser1Hash, false);

  std::string relative_path =
      base::FilePath(kTestUser1Hash).Append(FILE_PATH_LITERAL("DUMMY")).value();
  // Saves wallpaper info to local state for user |kTestUser1|.
  WallpaperInfo info = {
      relative_path,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  WallpaperManager::Get()->SetUserWallpaperInfo(kTestUser1, info, true);
  ResetUserManager();

  AppendGuestSwitch();
  scoped_ptr<WallpaperManager::TestApi> test_api;
  test_api.reset(new WallpaperManager::TestApi(WallpaperManager::Get()));
  // If last logged in user's wallpaper is used in function InitializeWallpaper,
  // this test will crash. InitializeWallpaper should be a noop after
  // AppendGuestSwitch being called.
  WallpaperManager::Get()->InitializeWallpaper();
  EXPECT_TRUE(test_api->current_wallpaper_path().empty());
  UserManager::Get()->UserLoggedIn(UserManager::kGuestUserName,
                                   UserManager::kGuestUserName, false);
  WaitAsyncWallpaperLoad();
  EXPECT_FALSE(ash::Shell::GetInstance()->desktop_background_controller()->
      SetDefaultWallpaper(true));
}

class WallpaperManagerCacheTest : public test::AshTestBase {
 public:
  WallpaperManagerCacheTest()
      : fake_user_manager_(new FakeUserManager()),
        scoped_user_manager_(fake_user_manager_) {
  }

 protected:
  virtual ~WallpaperManagerCacheTest() {}

  FakeUserManager* fake_user_manager() { return fake_user_manager_; }

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kMultiProfiles);
    test::AshTestBase::SetUp();
  }

  // Creates a test image of size 1x1.
  gfx::ImageSkia CreateTestImage(SkColor color) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    bitmap.allocPixels();
    bitmap.eraseColor(color);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

 private:
  FakeUserManager* fake_user_manager_;
  ScopedUserManagerEnabler scoped_user_manager_;
};

TEST_F(WallpaperManagerCacheTest, VerifyWallpaperCache) {
  // Add three users to known users.
  std::string test_user_1 = "test1@example.com";
  std::string test_user_2 = "test2@example.com";
  std::string test_user_3 = "test3@example.com";
  fake_user_manager()->AddUser(test_user_1);
  fake_user_manager()->AddUser(test_user_2);
  fake_user_manager()->AddUser(test_user_3);

  // Login two users.
  fake_user_manager()->LoginUser(test_user_1);
  fake_user_manager()->LoginUser(test_user_2);

  scoped_ptr<WallpaperManager::TestApi> test_api;
  test_api.reset(new WallpaperManager::TestApi(WallpaperManager::Get()));

  gfx::ImageSkia test_user_1_wallpaper = CreateTestImage(SK_ColorRED);
  gfx::ImageSkia test_user_2_wallpaper = CreateTestImage(SK_ColorGREEN);
  gfx::ImageSkia test_user_3_wallpaper = CreateTestImage(SK_ColorWHITE);
  test_api->SetWallpaperCache(test_user_1, test_user_1_wallpaper);
  test_api->SetWallpaperCache(test_user_2, test_user_2_wallpaper);
  test_api->SetWallpaperCache(test_user_3, test_user_3_wallpaper);

  test_api->ClearWallpaperCache();

  gfx::ImageSkia cached_wallpaper;
  EXPECT_TRUE(test_api->GetWallpaperFromCache(test_user_1, &cached_wallpaper));
  // Logged in users' wallpaper cache should be kept.
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(test_user_1_wallpaper));
  EXPECT_TRUE(test_api->GetWallpaperFromCache(test_user_2, &cached_wallpaper));
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(test_user_2_wallpaper));

  // Not logged in user's wallpaper cache should be cleared.
  EXPECT_FALSE(test_api->GetWallpaperFromCache(test_user_3, &cached_wallpaper));
}

}  // namespace chromeos

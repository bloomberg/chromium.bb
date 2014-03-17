// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include "ash/ash_resources/grit/ash_resources.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/display_manager_test_api.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"

using namespace ash;

namespace chromeos {

namespace {

const int kLargeWallpaperResourceId = IDR_AURA_WALLPAPER_DEFAULT_LARGE;
const int kSmallWallpaperResourceId = IDR_AURA_WALLPAPER_DEFAULT_SMALL;

int kLargeWallpaperWidth = 256;
int kLargeWallpaperHeight = ash::kLargeWallpaperMaxHeight;
int kSmallWallpaperWidth = 256;
int kSmallWallpaperHeight = ash::kSmallWallpaperMaxHeight;

const char kTestUser1[] = "test1@domain.com";
const char kTestUser1Hash[] = "test1@domain.com-hash";
const char kTestUser2[] = "test2@domain.com";
const char kTestUser2Hash[] = "test2@domain.com-hash";

}  // namespace

class WallpaperManagerBrowserTest : public InProcessBrowserTest,
                                    public DesktopBackgroundControllerObserver,
                                    public testing::WithParamInterface<bool> {
 public:
  WallpaperManagerBrowserTest () : controller_(NULL),
                                   local_state_(NULL) {
  }

  virtual ~WallpaperManagerBrowserTest () {}

  virtual void SetUpOnMainThread() OVERRIDE {
    controller_ = ash::Shell::GetInstance()->desktop_background_controller();
    controller_->AddObserver(this);
    local_state_ = g_browser_process->local_state();
    UpdateDisplay("800x600");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    if (GetParam())
      command_line->AppendSwitch(::switches::kMultiProfiles);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    controller_->RemoveObserver(this);
    controller_ = NULL;
  }

  // Update the display configuration as given in |display_specs|.
  // See ash::test::DisplayManagerTestApi::UpdateDisplay for more
  // details.
  void UpdateDisplay(const std::string& display_specs) {
    ash::test::DisplayManagerTestApi display_manager_test_api(
        ash::Shell::GetInstance()->display_manager());
    display_manager_test_api.UpdateDisplay(display_specs);
  }

  void WaitAsyncWallpaperLoadStarted() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  void WaitAsyncWallpaperLoadFinished() {
    base::MessageLoop::current()->RunUntilIdle();
    while (WallpaperManager::Get()->loading_.size()) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
      base::MessageLoop::current()->RunUntilIdle();
    }
  }

  virtual void OnWallpaperDataChanged() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

 protected:
  // Return custom wallpaper path. Create directory if not exist.
  base::FilePath GetCustomWallpaperPath(const char* sub_dir,
                                        const std::string& username_hash,
                                        const std::string& id) {
    base::FilePath wallpaper_path =
        WallpaperManager::Get()->GetCustomWallpaperPath(sub_dir,
                                                        username_hash,
                                                        id);
    if (!base::DirectoryExists(wallpaper_path.DirName()))
      base::CreateDirectory(wallpaper_path.DirName());

    return wallpaper_path;
  }

  // Logs in |username|.
  void LogIn(const std::string& username, const std::string& username_hash) {
    UserManager::Get()->UserLoggedIn(username, username_hash, false);
    WaitAsyncWallpaperLoadStarted();
  }

  // Saves bitmap |resource_id| to disk.
  void SaveUserWallpaperData(const base::FilePath& wallpaper_path,
                             int resource_id) {
    scoped_refptr<base::RefCountedStaticMemory> image_data(
        ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            resource_id, ui::SCALE_FACTOR_100P));
    int written = base::WriteFile(
        wallpaper_path,
        reinterpret_cast<const char*>(image_data->front()),
        image_data->size());
    EXPECT_EQ(static_cast<int>(image_data->size()), written);
  }

  int LoadedWallpapers() {
    return WallpaperManager::Get()->loaded_wallpapers();
  }

  DesktopBackgroundController* controller_;
  PrefService* local_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperManagerBrowserTest);
};

// Tests that the appropriate custom wallpaper (large vs. small) is loaded
// depending on the desktop resolution.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       LoadCustomLargeWallpaperForLargeExternalScreen) {
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  LogIn(kTestUser1, kTestUser1Hash);
  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      kSmallWallpaperSubDir,
      kTestUser1Hash,
      id);
  base::FilePath large_wallpaper_path = GetCustomWallpaperPath(
      kLargeWallpaperSubDir,
      kTestUser1Hash,
      id);

  // Saves the small/large resolution wallpapers to small/large custom
  // wallpaper paths.
  SaveUserWallpaperData(small_wallpaper_path,
                        kSmallWallpaperResourceId);
  SaveUserWallpaperData(large_wallpaper_path,
                        kLargeWallpaperResourceId);

  std::string relative_path = base::FilePath(kTestUser1Hash).Append(id).value();
  // Saves wallpaper info to local state for user |kTestUser1|.
  WallpaperInfo info = {
      relative_path,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(kTestUser1, info, true);

  // Set the wallpaper for |kTestUser1|.
  wallpaper_manager->SetUserWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadFinished();
  gfx::ImageSkia wallpaper = controller_->GetWallpaper();

  // Display is initialized to 800x600. The small resolution custom wallpaper is
  // expected.
  EXPECT_EQ(kSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kSmallWallpaperHeight, wallpaper.height());

  // Hook up another 800x600 display. This shouldn't trigger a reload.
  UpdateDisplay("800x600,800x600");
  content::RunAllPendingInMessageLoop();
  // The small resolution custom wallpaper is expected.
  EXPECT_EQ(kSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kSmallWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  UpdateDisplay("800x600,2000x2000");
  WaitAsyncWallpaperLoadFinished();
  wallpaper = controller_->GetWallpaper();

  // The large resolution custom wallpaper is expected.
  EXPECT_EQ(kLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kLargeWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up the 2000x2000 display again. The large resolution default wallpaper
  // should persist. Test for crbug/165788.
  UpdateDisplay("800x600,2000x2000");
  WaitAsyncWallpaperLoadFinished();
  wallpaper = controller_->GetWallpaper();

  // The large resolution custom wallpaper is expected.
  EXPECT_EQ(kLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kLargeWallpaperHeight, wallpaper.height());
}

// If chrome tries to reload the same wallpaper twice, the latter request should
// be prevented. Otherwise, there are some strange animation issues as
// described in crbug.com/158383.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       PreventReloadingSameWallpaper) {
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1, kTestUser1Hash);
  EXPECT_EQ(1, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(1, LoadedWallpapers());
  // Loads the same wallpaper after the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(1, LoadedWallpapers());
  wallpaper_manager->ClearDisposableWallpaperCache();

  // Change wallpaper to a custom wallpaper.
  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      kSmallWallpaperSubDir,
      kTestUser1Hash,
      id);
  SaveUserWallpaperData(small_wallpaper_path,
                        kSmallWallpaperResourceId);

  std::string relative_path = base::FilePath(kTestUser1Hash).Append(id).value();
  // Saves wallpaper info to local state for user |kTestUser1|.
  WallpaperInfo info = {
      relative_path,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(kTestUser1, info, true);

  wallpaper_manager->SetUserWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadStarted();
  EXPECT_EQ(2, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadStarted();
  EXPECT_EQ(2, LoadedWallpapers());
  wallpaper_manager->SetUserWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(2, LoadedWallpapers());
}

// Some users have old user profiles which may have legacy wallpapers. And these
// lagacy wallpapers should migrate to new wallpaper picker version seamlessly.
// This tests make sure we compatible with migrated old wallpapers.
// crosbug.com/38429
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       PRE_UseMigratedWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1, kTestUser1Hash);
  // Old wallpaper migration code doesn't exist in codebase anymore. Modify user
  // wallpaper info directly to simulate the wallpaper migration. See
  // crosbug.com/38429 for details about why we modify wallpaper info this way.
  WallpaperInfo info = {
      "123",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::DEFAULT,
      base::Time::Now().LocalMidnight()
  };
  base::FilePath user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  SaveUserWallpaperData(user_data_dir.Append("123"),
                        kLargeWallpaperResourceId);
  WallpaperManager::Get()->SetUserWallpaperInfo(kTestUser1, info, true);
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       UseMigratedWallpaperInfo) {
  LogIn(kTestUser1, kTestUser1Hash);
  WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because
  // migrated wallpaper is somehow not loaded. Bad things can happen if
  // wallpaper is not loaded at login screen. One example is: crosbug.com/38429.
}

// Some users have old user profiles which may never get a chance to migrate.
// This tests make sure we compatible with these profiles.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       PRE_UsePreMigrationWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1, kTestUser1Hash);
  // Old wallpaper migration code doesn't exist in codebase anymore. So if
  // user's profile is not migrated, it is the same as no wallpaper info. To
  // simulate this, we remove user's wallpaper info here.
  WallpaperManager::Get()->RemoveUserWallpaperInfo(kTestUser1);
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       UsePreMigrationWallpaperInfo) {
  LogIn(kTestUser1, kTestUser1Hash);
  WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because chrome
  // can not handle pre migrated user profile (M21 profile or older).
}

// Test for http://crbug.com/265689. When hooked up a large external monitor,
// the default large resolution wallpaper should load.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       HotPlugInScreenAtGAIALoginScreen) {
  UpdateDisplay("800x600");
  // Set initial wallpaper to the default wallpaper.
  WallpaperManager::Get()->SetDefaultWallpaperNow(UserManager::kStubUser);
  WaitAsyncWallpaperLoadFinished();

  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  UpdateDisplay("800x600,2000x2000");
  WaitAsyncWallpaperLoadFinished();
}

class WallpaperManagerBrowserTestNoAnimation
    : public WallpaperManagerBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(chromeos::switches::kDisableLoginAnimations);
    command_line->AppendSwitch(chromeos::switches::kDisableBootAnimation);
  }
};

// Same test as WallpaperManagerBrowserTest.UseMigratedWallpaperInfo. But
// disabled boot and login animation.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestNoAnimation,
                       PRE_UseMigratedWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1, kTestUser1Hash);
  // Old wallpaper migration code doesn't exist in codebase anymore. Modify user
  // wallpaper info directly to simulate the wallpaper migration. See
  // crosbug.com/38429 for details about why we modify wallpaper info this way.
  WallpaperInfo info = {
      "123",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::DEFAULT,
      base::Time::Now().LocalMidnight()
  };
  base::FilePath user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  SaveUserWallpaperData(user_data_dir.Append("123"),
                        kLargeWallpaperResourceId);
  WallpaperManager::Get()->SetUserWallpaperInfo(kTestUser1, info, true);
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestNoAnimation,
                       UseMigratedWallpaperInfo) {
  LogIn(kTestUser1, kTestUser1Hash);
  WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because
  // migrated wallpaper is somehow not loaded. Bad things can happen if
  // wallpaper is not loaded at login screen. One example is: crosbug.com/38429.
}

// Same test as WallpaperManagerBrowserTest.UsePreMigrationWallpaperInfo. But
// disabled boot and login animation.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestNoAnimation,
                       PRE_UsePreMigrationWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1, kTestUser1Hash);
  WaitAsyncWallpaperLoadFinished();
  // Old wallpaper migration code doesn't exist in codebase anymore. So if
  // user's profile is not migrated, it is the same as no wallpaper info. To
  // simulate this, we remove user's wallpaper info here.
  WallpaperManager::Get()->RemoveUserWallpaperInfo(kTestUser1);
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestNoAnimation,
                       UsePreMigrationWallpaperInfo) {
  LogIn(kTestUser1, kTestUser1Hash);
  WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because chrome
  // can not handle pre migrated user profile (M21 profile or older).
}

class WallpaperManagerBrowserTestCrashRestore
    : public WallpaperManagerBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kDisableLoginAnimations);
    command_line->AppendSwitch(chromeos::switches::kDisableBootAnimation);
    command_line->AppendSwitch(::switches::kMultiProfiles);
    command_line->AppendSwitchASCII(switches::kLoginUser, kTestUser1);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
        CryptohomeClient::GetStubSanitizedUsername(kTestUser1));
  }
};

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestCrashRestore,
                       PRE_RestoreWallpaper) {
  LogIn(kTestUser1, kTestUser1Hash);
  WaitAsyncWallpaperLoadFinished();
}

// Test for crbug.com/270278. It simulates a browser crash and verifies if user
// wallpaper is loaded.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestCrashRestore,
                       RestoreWallpaper) {
  EXPECT_EQ(1, LoadedWallpapers());
}

class WallpaperManagerBrowserTestCacheUpdate
    : public WallpaperManagerBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(::switches::kMultiProfiles);
    command_line->AppendSwitchASCII(switches::kLoginUser, kTestUser1);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
        CryptohomeClient::GetStubSanitizedUsername(kTestUser1));
  }
 protected:
  // Creates a test image of size 1x1.
  gfx::ImageSkia CreateTestImage(SkColor color) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    bitmap.allocPixels();
    bitmap.eraseColor(color);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }
};

// Sets kTestUser1's wallpaper to a custom wallpaper.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestCacheUpdate,
                       PRE_VerifyWallpaperCache) {
  // Add kTestUser1 to user list. kTestUser1 is the default login profile.
  LogIn(kTestUser1, kTestUser1Hash);

  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      kSmallWallpaperSubDir,
      kTestUser1Hash,
      id);
  base::FilePath large_wallpaper_path = GetCustomWallpaperPath(
      kLargeWallpaperSubDir,
      kTestUser1Hash,
      id);

  // Saves the small/large resolution wallpapers to small/large custom
  // wallpaper paths.
  SaveUserWallpaperData(small_wallpaper_path,
                        kSmallWallpaperResourceId);
  SaveUserWallpaperData(large_wallpaper_path,
                        kLargeWallpaperResourceId);

  std::string relative_path = base::FilePath(kTestUser1Hash).Append(id).value();
  // Saves wallpaper info to local state for user |kTestUser1|.
  WallpaperInfo info = {
      relative_path,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(kTestUser1, info, true);
  wallpaper_manager->SetUserWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadFinished();
  scoped_ptr<WallpaperManager::TestApi> test_api;
  test_api.reset(new WallpaperManager::TestApi(wallpaper_manager));
  // Verify SetUserWallpaperNow updates wallpaper cache.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_TRUE(test_api->GetWallpaperFromCache(kTestUser1, &cached_wallpaper));
}

// Tests for crbug.com/339576. Wallpaper cache should be updated in
// multi-profile mode when user:
// 1. chooses an online wallpaper from wallpaper
//    picker(calls SetWallpaperFromImageSkia);
// 2. chooses a custom wallpaper from wallpaper
//    picker(calls SetCustomWallpaper);
// 3. reverts to a default wallpaper.
// Also, when user login at multi-profile mode, previous logged in users'
// wallpaper cache should not be deleted.
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTestCacheUpdate,
                       VerifyWallpaperCache) {
  WaitAsyncWallpaperLoadFinished();
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  scoped_ptr<WallpaperManager::TestApi> test_api;
  test_api.reset(new WallpaperManager::TestApi(wallpaper_manager));
  gfx::ImageSkia cached_wallpaper;
  // Previous custom wallpaper should be cached after user login.
  EXPECT_TRUE(test_api->GetWallpaperFromCache(kTestUser1, &cached_wallpaper));

  LogIn(kTestUser2, kTestUser2Hash);
  WaitAsyncWallpaperLoadFinished();
  // Login another user should not delete logged in user's wallpaper cache.
  // Note active user is still kTestUser1.
  EXPECT_TRUE(test_api->GetWallpaperFromCache(kTestUser1, &cached_wallpaper));

  gfx::ImageSkia red_wallpaper = CreateTestImage(SK_ColorRED);
  wallpaper_manager->SetWallpaperFromImageSkia(kTestUser1,
                                               red_wallpaper,
                                               WALLPAPER_LAYOUT_CENTER,
                                               true);
  WaitAsyncWallpaperLoadFinished();
  // SetWallpaperFromImageSkia should update wallpaper cache when multi-profile
  // is turned on.
  EXPECT_TRUE(test_api->GetWallpaperFromCache(kTestUser1, &cached_wallpaper));
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(red_wallpaper));

  gfx::ImageSkia green_wallpaper = CreateTestImage(SK_ColorGREEN);
  chromeos::UserImage image(green_wallpaper);
  wallpaper_manager->SetCustomWallpaper(kTestUser1,
                                        kTestUser1Hash,
                                        "dummy",  // dummy file name
                                        WALLPAPER_LAYOUT_CENTER,
                                        User::CUSTOMIZED,
                                        image,
                                        true);
  WaitAsyncWallpaperLoadFinished();
  // SetCustomWallpaper should also update wallpaper cache when multi-profile is
  // turned on.
  EXPECT_TRUE(test_api->GetWallpaperFromCache(kTestUser1, &cached_wallpaper));
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(green_wallpaper));

  wallpaper_manager->SetDefaultWallpaperNow(kTestUser1);
  WaitAsyncWallpaperLoadFinished();
  // SetDefaultWallpaper should invalidate the user's wallpaper cache.
  EXPECT_FALSE(test_api->GetWallpaperFromCache(kTestUser1, &cached_wallpaper));
}

INSTANTIATE_TEST_CASE_P(WallpaperManagerBrowserTestInstantiation,
                        WallpaperManagerBrowserTest,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(WallpaperManagerBrowserTestNoAnimationInstantiation,
                        WallpaperManagerBrowserTestNoAnimation,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(WallpaperManagerBrowserTestCrashRestoreInstantiation,
                        WallpaperManagerBrowserTestCrashRestore,
                        testing::Bool());

INSTANTIATE_TEST_CASE_P(WallpaperManagerBrowserTestCacheUpdateInstantiation,
                        WallpaperManagerBrowserTestCacheUpdate,
                        testing::Bool());

}  // namespace chromeos

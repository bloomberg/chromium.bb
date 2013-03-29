// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/display_manager_test_api.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"

using namespace ash;

namespace chromeos {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
int kExpectedSmallWallpaperWidth = ash::kSmallWallpaperMaxWidth;
int kExpectedSmallWallpaperHeight = ash::kSmallWallpaperMaxHeight;
int kExpectedLargeWallpaperWidth = ash::kLargeWallpaperMaxWidth;
int kExpectedLargeWallpaperHeight = ash::kLargeWallpaperMaxHeight;
#else
// The defualt wallpaper for non official build is a gradient wallpaper which
// stretches to fit screen.
int kExpectedSmallWallpaperWidth = 256;
int kExpectedSmallWallpaperHeight = ash::kSmallWallpaperMaxHeight;
int kExpectedLargeWallpaperWidth = 256;
int kExpectedLargeWallpaperHeight = ash::kLargeWallpaperMaxHeight;
#endif

const char kTestUser1[] = "test@domain.com";

}  // namespace

class WallpaperManagerBrowserTest : public CrosInProcessBrowserTest,
                                    public DesktopBackgroundControllerObserver {
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

  void WaitAsyncWallpaperLoad() {
    MessageLoop::current()->Run();
  }

  virtual void OnWallpaperDataChanged() OVERRIDE {
    MessageLoop::current()->Quit();
  }

 protected:
  // Return custom wallpaper path. Create directory if not exist.
  base::FilePath GetCustomWallpaperPath(const char* sub_dir,
                                  const std::string& email,
                                  const std::string& id) {
    base::FilePath wallpaper_path =
        WallpaperManager::Get()->GetCustomWallpaperPath(sub_dir, email, id);
    if (!file_util::DirectoryExists(wallpaper_path.DirName()))
      file_util::CreateDirectory(wallpaper_path.DirName());

    return wallpaper_path;
  }

  // Logs in |username|.
  void LogIn(const std::string& username) {
    UserManager::Get()->UserLoggedIn(username, username, false);
  }

  // Saves bitmap |resource_id| to disk.
  void SaveUserWallpaperData(const std::string& username,
                             const base::FilePath& wallpaper_path,
                             int resource_id) {
    scoped_refptr<base::RefCountedStaticMemory> image_data(
        ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            resource_id, ui::SCALE_FACTOR_100P));
    int written = file_util::WriteFile(
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

// The large resolution wallpaper should be loaded when a large external screen
// is hooked up. If the external screen is smaller than small wallpaper
// resolution, do not load large resolution wallpaper.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       LoadLargeWallpaperForLargeExternalScreen) {
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  gfx::ImageSkia wallpaper = controller_->GetWallpaper();

  // Display is initialized to 800x600. The small resolution default wallpaper
  // is expected.
  EXPECT_EQ(kExpectedSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedSmallWallpaperHeight, wallpaper.height());

  // Hook up another 800x600 display.
  UpdateDisplay("800x600,800x600");
#if !defined(GOOGLE_CHROME_BUILD)
  // wallpaper.width() < 800, expect to reload wallpaper.
  WaitAsyncWallpaperLoad();
#endif
  // The small resolution wallpaper is expected.
  EXPECT_EQ(kExpectedSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedSmallWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up a 2000x2000 display. The large resolution default wallpaper should
  // be loaded.
  UpdateDisplay("800x600,2000x2000");
  WaitAsyncWallpaperLoad();
  wallpaper = controller_->GetWallpaper();

  // The large resolution default wallpaper is expected.
  EXPECT_EQ(kExpectedLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedLargeWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up the 2000x2000 display again. The large resolution default wallpaper
  // should persist. Test for crbug/165788.
  UpdateDisplay("800x600,2000x2000");
  WaitAsyncWallpaperLoad();
  wallpaper = controller_->GetWallpaper();

  // The large resolution default wallpaper is expected.
  EXPECT_EQ(kExpectedLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedLargeWallpaperHeight, wallpaper.height());
}

// This test is similar to LoadLargeWallpaperForExternalScreen test. Instead of
// testing default wallpaper, it tests custom wallpaper.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       LoadCustomLargeWallpaperForLargeExternalScreen) {
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  LogIn(kTestUser1);
  // Wait for default wallpaper loaded.
  WaitAsyncWallpaperLoad();
  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      kSmallWallpaperSubDir,
      kTestUser1,
      id);
  base::FilePath large_wallpaper_path = GetCustomWallpaperPath(
      kLargeWallpaperSubDir,
      kTestUser1,
      id);

  // Saves the small/large resolution wallpapers to small/large custom
  // wallpaper paths.
  SaveUserWallpaperData(kTestUser1,
                        small_wallpaper_path,
                        ash::kDefaultSmallWallpaper.idr);
  SaveUserWallpaperData(kTestUser1,
                        large_wallpaper_path,
                        ash::kDefaultLargeWallpaper.idr);

  // Saves wallpaper info to local state for user |kTestUser1|.
  WallpaperInfo info = {
      id,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(kTestUser1, info, true);

  // Set the wallpaper for |kTestUser1|.
  wallpaper_manager->SetUserWallpaper(kTestUser1);
  WaitAsyncWallpaperLoad();
  gfx::ImageSkia wallpaper = controller_->GetWallpaper();

  // Display is initialized to 800x600. The small resolution custom wallpaper is
  // expected.
  EXPECT_EQ(kExpectedSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedSmallWallpaperHeight, wallpaper.height());

  // Hook up another 800x600 display.
  UpdateDisplay("800x600,800x600");
#if !defined(GOOGLE_CHROME_BUILD)
  // wallpaper.width() < 800, expect to reload wallpaper.
  WaitAsyncWallpaperLoad();
#endif
  // The small resolution custom wallpaper is expected.
  EXPECT_EQ(kExpectedSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedSmallWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  UpdateDisplay("800x600,2000x2000");
  WaitAsyncWallpaperLoad();
  wallpaper = controller_->GetWallpaper();

  // The large resolution custom wallpaper is expected.
  EXPECT_EQ(kExpectedLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedLargeWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up the 2000x2000 display again. The large resolution default wallpaper
  // should persist. Test for crbug/165788.
  UpdateDisplay("800x600,2000x2000");
  WaitAsyncWallpaperLoad();
  wallpaper = controller_->GetWallpaper();

  // The large resolution custom wallpaper is expected.
  EXPECT_EQ(kExpectedLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedLargeWallpaperHeight, wallpaper.height());
}

// If chrome tries to reload the same wallpaper twice, the latter request should
// be prevented. Otherwise, there are some strange animation issues as
// described in crbug.com/158383.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       PreventReloadingSameWallpaper) {
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1);
  EXPECT_EQ(1, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaper(kTestUser1);
  EXPECT_EQ(1, LoadedWallpapers());
  WaitAsyncWallpaperLoad();
  // Loads the same wallpaper after the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaper(kTestUser1);
  EXPECT_EQ(1, LoadedWallpapers());
  wallpaper_manager->ClearWallpaperCache();

  // Change wallpaper to a custom wallpaper.
  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      kSmallWallpaperSubDir,
      kTestUser1,
      id);
  SaveUserWallpaperData(kTestUser1,
                        small_wallpaper_path,
                        ash::kDefaultSmallWallpaper.idr);

  // Saves wallpaper info to local state for user |kTestUser1|.
  WallpaperInfo info = {
      id,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(kTestUser1, info, true);

  wallpaper_manager->SetUserWallpaper(kTestUser1);
  EXPECT_EQ(2, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaper(kTestUser1);
  EXPECT_EQ(2, LoadedWallpapers());
  WaitAsyncWallpaperLoad();
  wallpaper_manager->SetUserWallpaper(kTestUser1);
  EXPECT_EQ(2, LoadedWallpapers());
}

// Old custom wallpaper is stored in USER_DATA_DIR. New custom wallpapers will
// be stored in USER_CUSTOM_WALLPAPER_DIR. The migration is triggered when any
// of the user fall back to load custom wallpaper from old path.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       MoveCustomWallpaper) {
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();

  base::FilePath old_wallpaper_path = wallpaper_manager->
      GetOriginalWallpaperPathForUser(kTestUser1);
  SaveUserWallpaperData(kTestUser1,
                        old_wallpaper_path,
                        ash::kDefaultSmallWallpaper.idr);
  // Saves wallpaper info to local state for user |kTestUser1|.
  WallpaperInfo info = {
      "DUMMY",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(kTestUser1, info, true);
  wallpaper_manager->SetUserWallpaper(kTestUser1);
  WaitAsyncWallpaperLoad();
  EXPECT_EQ(2, LoadedWallpapers());
  wallpaper_manager->UpdateWallpaper();
  // Wait for wallpaper migration and refresh. Note: the migration is guarantee
  // to finish before wallpaper refresh finish. This is guarantted by sequence
  // worker pool we use.
  WaitAsyncWallpaperLoad();
  EXPECT_EQ(3, LoadedWallpapers());
  base::FilePath new_wallpaper_path = GetCustomWallpaperPath(
      kOriginalWallpaperSubDir, kTestUser1, "DUMMY");
  EXPECT_FALSE(file_util::PathExists(old_wallpaper_path));
  EXPECT_TRUE(file_util::PathExists(new_wallpaper_path));
}

// Some users have old user profiles which may have legacy wallpapers. And these
// lagacy wallpapers should migrate to new wallpaper picker version seamlessly.
// This tests make sure we compatible with migrated old wallpapers.
// crosbug.com/38429
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       PRE_UseMigratedWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // Old wallpaper migration code doesn't exist in codebase anymore. Modify user
  // wallpaper info directly to simulate the wallpaper migration. See
  // crosbug.com/38429 for details about why we modify wallpaper info this way.
  WallpaperInfo info = {
      "123",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::DEFAULT,
      base::Time::Now().LocalMidnight()
  };
  WallpaperManager::Get()->SetUserWallpaperInfo(kTestUser1, info, true);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       UseMigratedWallpaperInfo) {
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // This test should finish normally. If timeout, it is probably because
  // migrated wallpaper is somehow not loaded. Bad things can happen if
  // wallpaper is not loaded at login screen. One example is: crosbug.com/38429.
}

// Some users have old user profiles which may never get a chance to migrate.
// This tests make sure we compatible with these profiles.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       PRE_UsePreMigrationWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // Old wallpaper migration code doesn't exist in codebase anymore. So if
  // user's profile is not migrated, it is the same as no wallpaper info. To
  // simulate this, we remove user's wallpaper info here.
  WallpaperManager::Get()->RemoveUserWallpaperInfo(kTestUser1);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       UsePreMigrationWallpaperInfo) {
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // This test should finish normally. If timeout, it is probably because chrome
  // can not handle pre migrated user profile (M21 profile or older).
}

class WallpaperManagerBrowserTestNoAnimation
    : public WallpaperManagerBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kDisableLoginAnimations);
    command_line->AppendSwitch(switches::kDisableBootAnimation);
  }
};

// Same test as WallpaperManagerBrowserTest.UseMigratedWallpaperInfo. But
// disabled boot and login animation.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       PRE_UseMigratedWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // Old wallpaper migration code doesn't exist in codebase anymore. Modify user
  // wallpaper info directly to simulate the wallpaper migration. See
  // crosbug.com/38429 for details about why we modify wallpaper info this way.
  WallpaperInfo info = {
      "123",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::DEFAULT,
      base::Time::Now().LocalMidnight()
  };
  WallpaperManager::Get()->SetUserWallpaperInfo(kTestUser1, info, true);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       UseMigratedWallpaperInfo) {
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // This test should finish normally. If timeout, it is probably because
  // migrated wallpaper is somehow not loaded. Bad things can happen if
  // wallpaper is not loaded at login screen. One example is: crosbug.com/38429.
}

// Same test as WallpaperManagerBrowserTest.UsePreMigrationWallpaperInfo. But
// disabled boot and login animation.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       PRE_UsePreMigrationWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // Old wallpaper migration code doesn't exist in codebase anymore. So if
  // user's profile is not migrated, it is the same as no wallpaper info. To
  // simulate this, we remove user's wallpaper info here.
  WallpaperManager::Get()->RemoveUserWallpaperInfo(kTestUser1);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       UsePreMigrationWallpaperInfo) {
  LogIn(kTestUser1);
  WaitAsyncWallpaperLoad();
  // This test should finish normally. If timeout, it is probably because chrome
  // can not handle pre migrated user profile (M21 profile or older).
}

}  // namepace chromeos

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/display_manager_test_api.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
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
  // Saves bitmap |resource_id| to disk.
  void SaveUserWallpaperData(const std::string& username,
                             const FilePath& wallpaper_path,
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
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();

  WallpaperInfo info = {
      "",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::DEFAULT,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(UserManager::kStubUser, info, true);

  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  WaitAsyncWallpaperLoad();
  gfx::ImageSkia wallpaper = controller_->GetWallpaper();

  // Display is initialized to 800x600. The small resolution default wallpaper
  // is expected.
  EXPECT_EQ(kExpectedSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedSmallWallpaperHeight, wallpaper.height());

  // Hook up another 800x600 display.
  UpdateDisplay("800x600,800x600");
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
  FilePath small_wallpaper_path =
      wallpaper_manager->GetWallpaperPathForUser(UserManager::kStubUser, true);
  FilePath large_wallpaper_path =
      wallpaper_manager->GetWallpaperPathForUser(UserManager::kStubUser, false);

  // Saves the small/large resolution wallpapers to small/large custom
  // wallpaper paths.
  SaveUserWallpaperData(UserManager::kStubUser,
                        small_wallpaper_path,
                        ash::kDefaultSmallWallpaper.idr);
  SaveUserWallpaperData(UserManager::kStubUser,
                        large_wallpaper_path,
                        ash::kDefaultLargeWallpaper.idr);

  // Saves wallpaper info to local state for user |UserManager::kStubUser|.
  WallpaperInfo info = {
      "DUMMY",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(UserManager::kStubUser, info, true);

  // Set the wallpaper for |UserManager::kStubUser|.
  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  WaitAsyncWallpaperLoad();
  gfx::ImageSkia wallpaper = controller_->GetWallpaper();

  // Display is initialized to 800x600. The small resolution custom wallpaper is
  // expected.
  EXPECT_EQ(kExpectedSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kExpectedSmallWallpaperHeight, wallpaper.height());

  // Hook up another 800x600 display.
  UpdateDisplay("800x600,800x600");
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
  FilePath small_wallpaper_path =
      wallpaper_manager->GetWallpaperPathForUser(UserManager::kStubUser, true);

  SaveUserWallpaperData(UserManager::kStubUser,
                        small_wallpaper_path,
                        ash::kDefaultSmallWallpaper.idr);

  // Saves wallpaper info to local state for user |UserManager::kStubUser|.
  WallpaperInfo info = {
      "DUMMY",
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      User::CUSTOMIZED,
      base::Time::Now().LocalMidnight()
  };
  wallpaper_manager->SetUserWallpaperInfo(UserManager::kStubUser, info, true);

  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  EXPECT_EQ(1, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  EXPECT_EQ(1, LoadedWallpapers());
  WaitAsyncWallpaperLoad();
  // Loads the same wallpaper after the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  EXPECT_EQ(1, LoadedWallpapers());
  wallpaper_manager->ClearWallpaperCache();

  // Tests default wallpaper for user |UserManager::kStubUser|.
  info.file = "";
  info.type = User::DEFAULT;
  wallpaper_manager->SetUserWallpaperInfo(UserManager::kStubUser, info, true);
  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  EXPECT_EQ(2, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  EXPECT_EQ(2, LoadedWallpapers());
  WaitAsyncWallpaperLoad();
  wallpaper_manager->SetUserWallpaper(UserManager::kStubUser);
  EXPECT_EQ(2, LoadedWallpapers());
}

}  // namepace chromeos

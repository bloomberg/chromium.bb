// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wallpaper_manager.h"

#include "ash/ash_resources/grit/ash_resources.h"
#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_controller_test_api.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_user_wallpaper_delegate.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/macros.h"
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
#include "content/public/test/test_utils.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

using namespace ash;

namespace chromeos {

namespace {

const int kLargeWallpaperResourceId = IDR_AURA_WALLPAPER_DEFAULT_LARGE;
const int kSmallWallpaperResourceId = IDR_AURA_WALLPAPER_DEFAULT_SMALL;

int kLargeWallpaperWidth = 256;
int kLargeWallpaperHeight = chromeos::kLargeWallpaperMaxHeight;
int kSmallWallpaperWidth = 256;
int kSmallWallpaperHeight = chromeos::kSmallWallpaperMaxHeight;

const char kTestUser1[] = "test1@domain.com";
const char kTestUser1Hash[] = "test1@domain.com-hash";
const char kTestUser2[] = "test2@domain.com";
const char kTestUser2Hash[] = "test2@domain.com-hash";

}  // namespace

class WallpaperManagerBrowserTest : public InProcessBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  WallpaperManagerBrowserTest () : controller_(NULL),
                                   local_state_(NULL) {
  }

  virtual ~WallpaperManagerBrowserTest () {}

  virtual void SetUpOnMainThread() OVERRIDE {
    controller_ = ash::Shell::GetInstance()->desktop_background_controller();
    local_state_ = g_browser_process->local_state();
    DesktopBackgroundController::TestAPI(controller_)
        .set_wallpaper_reload_delay_for_test(0);
    UpdateDisplay("800x600");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    if (GetParam())
      command_line->AppendSwitch(::switches::kMultiProfiles);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    controller_ = NULL;
  }

  // Update the display configuration as given in |display_specs|.  See
  // ash::test::DisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_specs) {
    ash::test::DisplayManagerTestApi display_manager_test_api(
        ash::Shell::GetInstance()->display_manager());
    display_manager_test_api.UpdateDisplay(display_specs);
  }

  void WaitAsyncWallpaperLoadStarted() {
    base::RunLoop().RunUntilIdle();
  }

  void WaitAsyncWallpaperLoadFinished() {
    base::RunLoop().RunUntilIdle();
    while (WallpaperManager::Get()->loading_.size()) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
      base::RunLoop().RunUntilIdle();
    }
  }

 protected:
  // Colors used for different default wallpapers by
  // WriteWallpapers().
  static const SkColor kLargeWallpaperColor = SK_ColorRED;
  static const SkColor kSmallWallpaperColor = SK_ColorGREEN;
  static const SkColor kLargeGuestWallpaperColor = SK_ColorBLUE;
  static const SkColor kSmallGuestWallpaperColor = SK_ColorYELLOW;

  // A color that can be passed to CreateImage(). Specifically chosen to not
  // conflict with any of the default wallpaper colors.
  static const SkColor kCustomWallpaperColor = SK_ColorMAGENTA;

  // Dimension used for width and height of default wallpaper images. A
  // small value is used to minimize the amount of time spent compressing
  // and writing images.
  static const int kWallpaperSize = 2;

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

  // Creates a test image of size 1x1.
  gfx::ImageSkia CreateTestImage(int width, int height, SkColor color) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseColor(color);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  // Writes a JPEG image of the specified size and color to |path|. Returns
  // true on success.
  bool WriteJPEGFile(const base::FilePath& path,
                     int width,
                     int height,
                     SkColor color) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height, 0);
    bitmap.allocPixels();
    bitmap.eraseColor(color);

    const int kQuality = 80;
    std::vector<unsigned char> output;
    if (!gfx::JPEGCodec::Encode(
            static_cast<const unsigned char*>(bitmap.getPixels()),
            gfx::JPEGCodec::FORMAT_SkBitmap,
            width,
            height,
            bitmap.rowBytes(),
            kQuality,
            &output)) {
      LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
      return false;
    }

    size_t bytes_written = base::WriteFile(
        path, reinterpret_cast<const char*>(&output[0]), output.size());
    if (bytes_written != output.size()) {
      LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
                 << output.size() << " to " << path.value();
      return false;
    }

    return true;
  }

  // Initializes default wallpaper paths "*default_*file" and writes JPEG
  // wallpaper images to them.
  // Only needs to be called (once) by tests that want to test loading of
  // default wallpapers.
  void WriteWallpapers() {
    wallpaper_dir_.reset(new base::ScopedTempDir);
    ASSERT_TRUE(wallpaper_dir_->CreateUniqueTempDir());

    std::vector<std::string> options;
    options.push_back(std::string("WM_Test_cmdline"));
    const base::FilePath small_file =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("small.jpg"));
    options.push_back(std::string("--") +
                      ash::switches::kAshDefaultWallpaperSmall + "=" +
                      small_file.value());
    const base::FilePath large_file =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("large.jpg"));
    options.push_back(std::string("--") +
                      ash::switches::kAshDefaultWallpaperLarge + "=" +
                      large_file.value());
    const base::FilePath guest_small_file =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("guest_small.jpg"));
    options.push_back(std::string("--") +
                      ash::switches::kAshGuestWallpaperSmall + "=" +
                      guest_small_file.value());
    const base::FilePath guest_large_file =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("guest_large.jpg"));
    options.push_back(std::string("--") +
                      ash::switches::kAshGuestWallpaperLarge + "=" +
                      guest_large_file.value());

    ASSERT_TRUE(WriteJPEGFile(small_file,
                              kWallpaperSize,
                              kWallpaperSize,
                              kSmallWallpaperColor));
    ASSERT_TRUE(WriteJPEGFile(large_file,
                              kWallpaperSize,
                              kWallpaperSize,
                              kLargeWallpaperColor));
    ASSERT_TRUE(WriteJPEGFile(guest_small_file,
                              kWallpaperSize,
                              kWallpaperSize,
                              kSmallGuestWallpaperColor));
    ASSERT_TRUE(WriteJPEGFile(guest_large_file,
                              kWallpaperSize,
                              kWallpaperSize,
                              kLargeGuestWallpaperColor));

    wallpaper_manager_command_line_.reset(new base::CommandLine(options));
    WallpaperManager::Get()->SetCommandLineForTesting(
        wallpaper_manager_command_line_.get());
  }

  // Returns true if the color at the center of |image| is close to
  // |expected_color|. (The center is used so small wallpaper images can be
  // used.)
  bool ImageIsNearColor(gfx::ImageSkia image, SkColor expected_color) {
    if (image.size().IsEmpty()) {
      LOG(ERROR) << "Image is empty";
      return false;
    }

    const SkBitmap* bitmap = image.bitmap();
    if (!bitmap) {
      LOG(ERROR) << "Unable to get bitmap from image";
      return false;
    }

    bitmap->lockPixels();
    gfx::Point center = gfx::Rect(image.size()).CenterPoint();
    SkColor image_color = bitmap->getColor(center.x(), center.y());
    bitmap->unlockPixels();

    const int kDiff = 3;
    if (std::abs(static_cast<int>(SkColorGetA(image_color)) -
                 static_cast<int>(SkColorGetA(expected_color))) > kDiff ||
        std::abs(static_cast<int>(SkColorGetR(image_color)) -
                 static_cast<int>(SkColorGetR(expected_color))) > kDiff ||
        std::abs(static_cast<int>(SkColorGetG(image_color)) -
                 static_cast<int>(SkColorGetG(expected_color))) > kDiff ||
        std::abs(static_cast<int>(SkColorGetB(image_color)) -
                 static_cast<int>(SkColorGetB(expected_color))) > kDiff) {
      LOG(ERROR) << "Expected color near 0x" << std::hex << expected_color
                 << " but got 0x" << image_color;
      return false;
    }

    return true;
  }

  DesktopBackgroundController* controller_;
  PrefService* local_state_;
  scoped_ptr<base::CommandLine> wallpaper_manager_command_line_;

  // Directory created by WriteWallpapersAndSetFlags() to store default
  // wallpaper images.
  scoped_ptr<base::ScopedTempDir> wallpaper_dir_;

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
  WaitAsyncWallpaperLoadFinished();
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
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
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
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
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
//    picker (calls SetWallpaperFromImageSkia);
// 2. chooses a custom wallpaper from wallpaper
//    picker (calls SetCustomWallpaper);
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

// ----------------------------------------------------------------------
// Test default wallpapers.

class TestObserver : public WallpaperManager::Observer {
 public:
  explicit TestObserver(WallpaperManager* wallpaper_manager)
      : update_wallpaper_count_(0), wallpaper_manager_(wallpaper_manager) {
    DCHECK(wallpaper_manager_);
    wallpaper_manager_->AddObserver(this);
  }

  virtual ~TestObserver() {
    wallpaper_manager_->RemoveObserver(this);
  }

  virtual void OnWallpaperAnimationFinished(const std::string&) OVERRIDE {
  }

  virtual void OnUpdateWallpaperForTesting() OVERRIDE {
    ++update_wallpaper_count_;
  }

  int GetUpdateWallpaperCountAndReset() {
    const size_t old = update_wallpaper_count_;
    update_wallpaper_count_ = 0;
    return old;
  }

 private:
  int update_wallpaper_count_;
  WallpaperManager* wallpaper_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest, DisplayChange) {
  // TODO(derat|oshima|bshe): Host windows can't be resized on Win8.
  if (!ash::test::AshTestHelper::SupportsHostWindowResize())
    return;

  TestObserver observer(WallpaperManager::Get());

  // Set the wallpaper to ensure that UpdateWallpaper() will be called when the
  // display configuration changes.
  gfx::ImageSkia image = CreateTestImage(640, 480, kCustomWallpaperColor);
  controller_->SetWallpaperImage(image, WALLPAPER_LAYOUT_STRETCH);

  // Small wallpaper images should be used for configurations less than or
  // equal to kSmallWallpaperMaxWidth by kSmallWallpaperMaxHeight, even if
  // multiple displays are connected.
  UpdateDisplay("800x600");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("800x600,800x600");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("1366x800");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // At larger sizes, large wallpapers should be used.
  UpdateDisplay("1367x800");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("1367x801");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("2560x1700");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // Rotated smaller screen may use larger image.
  UpdateDisplay("800x600/r");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("800x600/r,800x600");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());
  UpdateDisplay("1366x800/r");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // Max display size didn't chagne.
  UpdateDisplay("900x800/r,400x1366");
  WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());
}

// Test that WallpaperManager loads the appropriate wallpaper
// images as specified via command-line flags in various situations.
// Splitting these into separate tests avoids needing to run animations.
// TODO(derat): Combine these into a single test
IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest, SmallDefaultWallpaper) {
  if (!ash::test::AshTestHelper::SupportsMultipleDisplays())
    return;

  WriteWallpapers();

  // At 800x600, the small wallpaper should be loaded.
  UpdateDisplay("800x600");
  WallpaperManager::Get()->SetDefaultWallpaperNow(std::string());
  WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(
      ImageIsNearColor(controller_->GetWallpaper(), kSmallWallpaperColor));
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest, LargeDefaultWallpaper) {
  if (!ash::test::AshTestHelper::SupportsMultipleDisplays())
    return;

  WriteWallpapers();
  UpdateDisplay("1600x1200");
  WallpaperManager::Get()->SetDefaultWallpaperNow(std::string());
  WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(
      ImageIsNearColor(controller_->GetWallpaper(), kLargeWallpaperColor));
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       LargeDefaultWallpaperWhenRotated) {
  if (!ash::test::AshTestHelper::SupportsMultipleDisplays())
    return;
  WriteWallpapers();

  UpdateDisplay("1200x800/r");
  WallpaperManager::Get()->SetDefaultWallpaperNow(std::string());
  WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(
      ImageIsNearColor(controller_->GetWallpaper(), kLargeWallpaperColor));
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest, SmallGuestWallpaper) {
  if (!ash::test::AshTestHelper::SupportsMultipleDisplays())
    return;
  WriteWallpapers();
  UserManager::Get()->UserLoggedIn(
      UserManager::kGuestUserName, UserManager::kGuestUserName, false);
  UpdateDisplay("800x600");
  WallpaperManager::Get()->SetDefaultWallpaperNow(std::string());
  WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(
      ImageIsNearColor(controller_->GetWallpaper(), kSmallGuestWallpaperColor));
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest, LargeGuestWallpaper) {
  if (!ash::test::AshTestHelper::SupportsMultipleDisplays())
    return;

  WriteWallpapers();
  UserManager::Get()->UserLoggedIn(
      UserManager::kGuestUserName, UserManager::kGuestUserName, false);
  UpdateDisplay("1600x1200");
  WallpaperManager::Get()->SetDefaultWallpaperNow(std::string());
  WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(
      ImageIsNearColor(controller_->GetWallpaper(), kLargeGuestWallpaperColor));
}

IN_PROC_BROWSER_TEST_P(WallpaperManagerBrowserTest,
                       SwitchBetweenDefaultAndCustom) {
  // Start loading the default wallpaper.
  UpdateDisplay("640x480");
  WriteWallpapers();
  UserManager::Get()->UserLoggedIn(UserManager::kStubUser, "test_hash", false);

  WallpaperManager::Get()->SetDefaultWallpaperNow(std::string());

  // Custom wallpaper should be applied immediately, canceling the default
  // wallpaper load task.
  gfx::ImageSkia image = CreateTestImage(640, 480, kCustomWallpaperColor);
  UserImage wallpaper(image);
  WallpaperManager::Get()->SetCustomWallpaper(UserManager::kStubUser,
                                              "test_hash",
                                              "test-nofile.jpeg",
                                              WALLPAPER_LAYOUT_STRETCH,
                                              User::CUSTOMIZED,
                                              wallpaper,
                                              true);
  WaitAsyncWallpaperLoadFinished();

  EXPECT_TRUE(
      ImageIsNearColor(controller_->GetWallpaper(), kCustomWallpaperColor));

  WallpaperManager::Get()->SetDefaultWallpaperNow(std::string());
  WaitAsyncWallpaperLoadFinished();

  EXPECT_TRUE(
      ImageIsNearColor(controller_->GetWallpaper(), kSmallWallpaperColor));
}

}  // namespace chromeos

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"

#include <stddef.h>

#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/env.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

using session_manager::SessionManager;
using wallpaper::WallpaperInfo;
using wallpaper::WALLPAPER_LAYOUT_CENTER;
using wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
using wallpaper::WALLPAPER_LAYOUT_STRETCH;
using wallpaper::WALLPAPER_LAYOUT_TILE;

namespace chromeos {

namespace {

int kLargeWallpaperWidth = 256;
int kLargeWallpaperHeight = wallpaper::kLargeWallpaperMaxHeight;
int kSmallWallpaperWidth = 256;
int kSmallWallpaperHeight = wallpaper::kSmallWallpaperMaxHeight;

const char kTestUser1[] = "test1@domain.com";
const char kTestUser1Hash[] = "test1@domain.com-hash";
const char kTestUser2[] = "test2@domain.com";
const char kTestUser2Hash[] = "test2@domain.com-hash";

}  // namespace

class WallpaperManagerBrowserTest : public InProcessBrowserTest {
 public:
  WallpaperManagerBrowserTest() : controller_(NULL),
                                  local_state_(NULL) {
  }

  ~WallpaperManagerBrowserTest() override {}

  void SetUpOnMainThread() override {
    controller_ = ash::WmShell::Get()->wallpaper_controller();
    controller_->set_wallpaper_reload_delay_for_test(0);
    local_state_ = g_browser_process->local_state();
    UpdateDisplay("800x600");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  }

  void TearDownOnMainThread() override { controller_ = NULL; }

  // Update the display configuration as given in |display_specs|.  See
  // display::test::DisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_specs) {
    display::test::DisplayManagerTestApi(
        ash::Shell::GetInstance()->display_manager())
        .UpdateDisplay(display_specs);
  }

  void WaitAsyncWallpaperLoadStarted() {
    base::RunLoop().RunUntilIdle();
  }

 protected:

  // Return custom wallpaper path. Create directory if not exist.
  base::FilePath GetCustomWallpaperPath(
      const char* sub_dir,
      const wallpaper::WallpaperFilesId& wallpaper_files_id,
      const std::string& id) {
    base::FilePath wallpaper_path =
        WallpaperManager::Get()->GetCustomWallpaperPath(sub_dir,
                                                        wallpaper_files_id, id);
    if (!base::DirectoryExists(wallpaper_path.DirName()))
      base::CreateDirectory(wallpaper_path.DirName());

    return wallpaper_path;
  }

  // Logs in |account_id|.
  void LogIn(const AccountId& account_id, const std::string& user_id_hash) {
    SessionManager::Get()->CreateSession(account_id, user_id_hash);
    // Adding a secondary display creates a shelf on that display, which
    // assumes a shelf on the primary display if the user was logged in.
    ash::WmShell::Get()->CreateShelfView();
    WaitAsyncWallpaperLoadStarted();
  }

  // Logs in |account_id| and sets it as child account.
  void LogInAsChild(const AccountId& account_id,
                    const std::string& user_id_hash) {
    SessionManager::Get()->CreateSession(account_id, user_id_hash);
    user_manager::User* user =
        user_manager::UserManager::Get()->FindUserAndModify(account_id);
    user_manager::UserManager::Get()->ChangeUserChildStatus(
        user, true /* is_child */);
    // TODO(jamescook): For some reason creating the shelf here (which is what
    // would happen in normal login) causes the child wallpaper tests to fail
    // with the wallpaper having alpha. This looks like the wallpaper is mid-
    // animation, but happens even if animations are disabled. Something is
    // wrong with how these tests simulate login.
  }

  int LoadedWallpapers() {
    return WallpaperManager::Get()->loaded_wallpapers_for_test();
  }

  void CacheUserWallpaper(const AccountId& account_id) {
    WallpaperManager::Get()->CacheUserWallpaper(account_id);
  }

  void ClearDisposableWallpaperCache() {
    WallpaperManager::Get()->ClearDisposableWallpaperCache();
  }

  // Initializes default wallpaper paths "*default_*file" and writes JPEG
  // wallpaper images to them.
  // Only needs to be called (once) by tests that want to test loading of
  // default wallpapers.
  void CreateCmdlineWallpapers() {
    wallpaper_dir_.reset(new base::ScopedTempDir);
    ASSERT_TRUE(wallpaper_dir_->CreateUniqueTempDir());
    wallpaper_manager_test_utils::CreateCmdlineWallpapers(
        *wallpaper_dir_, &wallpaper_manager_command_line_);
  }

  ash::WallpaperController* controller_;
  PrefService* local_state_;
  std::unique_ptr<base::CommandLine> wallpaper_manager_command_line_;

  // Directory created by CreateCmdlineWallpapers () to store default
  // wallpaper images.
  std::unique_ptr<base::ScopedTempDir> wallpaper_dir_;

  const AccountId test_account_id1_ = AccountId::FromUserEmail(kTestUser1);
  const AccountId test_account_id2_ = AccountId::FromUserEmail(kTestUser2);

  const wallpaper::WallpaperFilesId test_account1_wallpaper_files_id_ =
      wallpaper::WallpaperFilesId::FromString(kTestUser1Hash);
  const wallpaper::WallpaperFilesId test_account2_wallpaper_files_id_ =
      wallpaper::WallpaperFilesId::FromString(kTestUser2Hash);

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperManagerBrowserTest);
};

// Tests that the appropriate custom wallpaper (large vs. small) is loaded
// depending on the desktop resolution.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       LoadCustomLargeWallpaperForLargeExternalScreen) {
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  LogIn(test_account_id1_, kTestUser1Hash);
  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      wallpaper::kSmallWallpaperSubDir, test_account1_wallpaper_files_id_, id);
  base::FilePath large_wallpaper_path = GetCustomWallpaperPath(
      wallpaper::kLargeWallpaperSubDir, test_account1_wallpaper_files_id_, id);

  // Saves the small/large resolution wallpapers to small/large custom
  // wallpaper paths.
  ASSERT_TRUE(wallpaper_manager_test_utils::WriteJPEGFile(
      small_wallpaper_path,
      kSmallWallpaperWidth,
      kSmallWallpaperHeight,
      wallpaper_manager_test_utils::kSmallDefaultWallpaperColor));
  ASSERT_TRUE(wallpaper_manager_test_utils::WriteJPEGFile(
      large_wallpaper_path,
      kLargeWallpaperWidth,
      kLargeWallpaperHeight,
      wallpaper_manager_test_utils::kLargeDefaultWallpaperColor));

  std::string relative_path =
      base::FilePath(test_account1_wallpaper_files_id_.id()).Append(id).value();
  // Saves wallpaper info to local state for user |test_account_id1_|.
  WallpaperInfo info = {relative_path, WALLPAPER_LAYOUT_CENTER_CROPPED,
                        user_manager::User::CUSTOMIZED,
                        base::Time::Now().LocalMidnight()};
  wallpaper_manager->SetUserWallpaperInfo(test_account_id1_, info, true);

  // Set the wallpaper for |test_account_id1_|.
  wallpaper_manager->SetUserWallpaperNow(test_account_id1_);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  gfx::ImageSkia wallpaper = controller_->GetWallpaper();

  // Display is initialized to 800x600. The small resolution custom wallpaper is
  // expected.
  EXPECT_EQ(kSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kSmallWallpaperHeight, wallpaper.height());

  // Hook up another 800x600 display. This shouldn't trigger a reload.
  UpdateDisplay("800x600,800x600");
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // The small resolution custom wallpaper is expected.
  EXPECT_EQ(kSmallWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kSmallWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  UpdateDisplay("800x600,2000x2000");
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  wallpaper = controller_->GetWallpaper();

  // The large resolution custom wallpaper is expected.
  EXPECT_EQ(kLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kLargeWallpaperHeight, wallpaper.height());

  // Detach the secondary display.
  UpdateDisplay("800x600");
  // Hook up the 2000x2000 display again. The large resolution default wallpaper
  // should persist. Test for crbug/165788.
  UpdateDisplay("800x600,2000x2000");
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  wallpaper = controller_->GetWallpaper();

  // The large resolution custom wallpaper is expected.
  EXPECT_EQ(kLargeWallpaperWidth, wallpaper.width());
  EXPECT_EQ(kLargeWallpaperHeight, wallpaper.height());
}

// If chrome tries to reload the same wallpaper twice, the latter request should
// be prevented. Otherwise, there are some strange animation issues as
// described in crbug.com/158383.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       PreventReloadingSameWallpaper) {
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  // New user log in, a default wallpaper is loaded.
  LogIn(test_account_id1_, kTestUser1Hash);
  EXPECT_EQ(1, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaperNow(test_account_id1_);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(1, LoadedWallpapers());
  // Loads the same wallpaper after the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaperNow(test_account_id1_);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(1, LoadedWallpapers());
  ClearDisposableWallpaperCache();

  // Change wallpaper to a custom wallpaper.
  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      wallpaper::kSmallWallpaperSubDir, test_account1_wallpaper_files_id_, id);
  ASSERT_TRUE(wallpaper_manager_test_utils::WriteJPEGFile(
      small_wallpaper_path,
      kSmallWallpaperWidth,
      kSmallWallpaperHeight,
      wallpaper_manager_test_utils::kSmallDefaultWallpaperColor));

  std::string relative_path =
      base::FilePath(test_account1_wallpaper_files_id_.id()).Append(id).value();
  // Saves wallpaper info to local state for user |test_account_id1_|.
  WallpaperInfo info = {relative_path, WALLPAPER_LAYOUT_CENTER_CROPPED,
                        user_manager::User::CUSTOMIZED,
                        base::Time::Now().LocalMidnight()};
  wallpaper_manager->SetUserWallpaperInfo(test_account_id1_, info, true);

  wallpaper_manager->SetUserWallpaperNow(test_account_id1_);
  WaitAsyncWallpaperLoadStarted();
  EXPECT_EQ(2, LoadedWallpapers());
  // Loads the same wallpaper before the initial one finished. It should be
  // prevented.
  wallpaper_manager->SetUserWallpaperNow(test_account_id1_);
  WaitAsyncWallpaperLoadStarted();
  EXPECT_EQ(2, LoadedWallpapers());
  wallpaper_manager->SetUserWallpaperNow(test_account_id1_);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(2, LoadedWallpapers());
}

// Some users have old user profiles which may have legacy wallpapers. And these
// lagacy wallpapers should migrate to new wallpaper picker version seamlessly.
// This tests make sure we compatible with migrated old wallpapers.
// crosbug.com/38429
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       PRE_UseMigratedWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(test_account_id1_, kTestUser1Hash);
  // Old wallpaper migration code doesn't exist in codebase anymore. Modify user
  // wallpaper info directly to simulate the wallpaper migration. See
  // crosbug.com/38429 for details about why we modify wallpaper info this way.
  WallpaperInfo info = {"123", WALLPAPER_LAYOUT_CENTER_CROPPED,
                        user_manager::User::DEFAULT,
                        base::Time::Now().LocalMidnight()};
  base::FilePath user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  ASSERT_TRUE(wallpaper_manager_test_utils::WriteJPEGFile(
      user_data_dir.Append("123"),
      wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kLargeDefaultWallpaperColor));
  WallpaperManager::Get()->SetUserWallpaperInfo(test_account_id1_, info, true);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       UseMigratedWallpaperInfo) {
  LogIn(test_account_id1_, kTestUser1Hash);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because
  // migrated wallpaper is somehow not loaded. Bad things can happen if
  // wallpaper is not loaded at login screen. One example is: crosbug.com/38429.
}

// Some users have old user profiles which may never get a chance to migrate.
// This tests make sure we compatible with these profiles.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       PRE_UsePreMigrationWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(test_account_id1_, kTestUser1Hash);
  // Old wallpaper migration code doesn't exist in codebase anymore. So if
  // user's profile is not migrated, it is the same as no wallpaper info. To
  // simulate this, we remove user's wallpaper info here.
  WallpaperManager::Get()->RemoveUserWallpaperInfo(test_account_id1_);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       UsePreMigrationWallpaperInfo) {
  LogIn(test_account_id1_, kTestUser1Hash);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because chrome
  // can not handle pre migrated user profile (M21 profile or older).
}

// Test for http://crbug.com/265689. When hooked up a large external monitor,
// the default large resolution wallpaper should load.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       HotPlugInScreenAtGAIALoginScreen) {
  UpdateDisplay("800x600");
  // Set initial wallpaper to the default wallpaper.
  WallpaperManager::Get()->SetDefaultWallpaperNow(
      user_manager::StubAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();

  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  UpdateDisplay("800x600,2000x2000");
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
}

class WallpaperManagerBrowserTestNoAnimation
    : public WallpaperManagerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(chromeos::switches::kDisableLoginAnimations);
    command_line->AppendSwitch(chromeos::switches::kDisableBootAnimation);
  }
};

// Same test as WallpaperManagerBrowserTest.UseMigratedWallpaperInfo. But
// disabled boot and login animation.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       PRE_UseMigratedWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(test_account_id1_, kTestUser1Hash);
  // Old wallpaper migration code doesn't exist in codebase anymore. Modify user
  // wallpaper info directly to simulate the wallpaper migration. See
  // crosbug.com/38429 for details about why we modify wallpaper info this way.
  WallpaperInfo info = {"123", WALLPAPER_LAYOUT_CENTER_CROPPED,
                        user_manager::User::DEFAULT,
                        base::Time::Now().LocalMidnight()};
  base::FilePath user_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  ASSERT_TRUE(wallpaper_manager_test_utils::WriteJPEGFile(
      user_data_dir.Append("123"),
      wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kWallpaperSize,
      wallpaper_manager_test_utils::kLargeDefaultWallpaperColor));
  WallpaperManager::Get()->SetUserWallpaperInfo(test_account_id1_, info, true);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       UseMigratedWallpaperInfo) {
  LogIn(test_account_id1_, kTestUser1Hash);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because
  // migrated wallpaper is somehow not loaded. Bad things can happen if
  // wallpaper is not loaded at login screen. One example is: crosbug.com/38429.
}

// Same test as WallpaperManagerBrowserTest.UsePreMigrationWallpaperInfo. But
// disabled boot and login animation.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       PRE_UsePreMigrationWallpaperInfo) {
  // New user log in, a default wallpaper is loaded.
  LogIn(test_account_id1_, kTestUser1Hash);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // Old wallpaper migration code doesn't exist in codebase anymore. So if
  // user's profile is not migrated, it is the same as no wallpaper info. To
  // simulate this, we remove user's wallpaper info here.
  WallpaperManager::Get()->RemoveUserWallpaperInfo(test_account_id1_);
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestNoAnimation,
                       UsePreMigrationWallpaperInfo) {
  LogIn(test_account_id1_, kTestUser1Hash);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // This test should finish normally. If timeout, it is probably because chrome
  // can not handle pre migrated user profile (M21 profile or older).
}

class WallpaperManagerBrowserTestCrashRestore
    : public WallpaperManagerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kDisableLoginAnimations);
    command_line->AppendSwitch(chromeos::switches::kDisableBootAnimation);
    command_line->AppendSwitchASCII(switches::kLoginUser,
                                    test_account_id1_.GetUserEmail());
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  }
};

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestCrashRestore,
                       PRE_RestoreWallpaper) {
  // No need to explicitly login for crash-n-restore.
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
}

// Test for crbug.com/270278. It simulates a browser crash and verifies if user
// wallpaper is loaded.
// Fails on the MSAN bots. See http://crbug.com/444477
#if defined(MEMORY_SANITIZER)
#define MAYBE_RestoreWallpaper DISABLED_RestoreWallpaper
#else
#define MAYBE_RestoreWallpaper RestoreWallpaper
#endif
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestCrashRestore,
                       MAYBE_RestoreWallpaper) {
  EXPECT_EQ(1, LoadedWallpapers());
}

class WallpaperManagerBrowserTestCacheUpdate
    : public WallpaperManagerBrowserTest {
 protected:
  // Creates a test image of size 1x1.
  gfx::ImageSkia CreateTestImage(SkColor color) {
    return wallpaper_manager_test_utils::CreateTestImage(1, 1, color);
  }
};

// Sets test_account_id1_'s wallpaper to a custom wallpaper.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestCacheUpdate,
                       PRE_VerifyWallpaperCache) {
  // Add test_account_id1_ to user list.
  // test_account_id1_ is the default login profile.
  LogIn(test_account_id1_, kTestUser1Hash);

  std::string id = base::Int64ToString(base::Time::Now().ToInternalValue());
  WallpaperManager* wallpaper_manager = WallpaperManager::Get();
  base::FilePath small_wallpaper_path = GetCustomWallpaperPath(
      wallpaper::kSmallWallpaperSubDir, test_account1_wallpaper_files_id_, id);
  base::FilePath large_wallpaper_path = GetCustomWallpaperPath(
      wallpaper::kLargeWallpaperSubDir, test_account1_wallpaper_files_id_, id);

  // Saves the small/large resolution wallpapers to small/large custom
  // wallpaper paths.
  ASSERT_TRUE(wallpaper_manager_test_utils::WriteJPEGFile(
      small_wallpaper_path,
      kSmallWallpaperWidth,
      kSmallWallpaperHeight,
      wallpaper_manager_test_utils::kSmallDefaultWallpaperColor));
  ASSERT_TRUE(wallpaper_manager_test_utils::WriteJPEGFile(
      large_wallpaper_path,
      kLargeWallpaperWidth,
      kLargeWallpaperHeight,
      wallpaper_manager_test_utils::kLargeDefaultWallpaperColor));

  std::string relative_path =
      base::FilePath(test_account1_wallpaper_files_id_.id()).Append(id).value();
  // Saves wallpaper info to local state for user |test_account_id1_|.
  WallpaperInfo info = {relative_path, WALLPAPER_LAYOUT_CENTER_CROPPED,
                        user_manager::User::CUSTOMIZED,
                        base::Time::Now().LocalMidnight()};
  wallpaper_manager->SetUserWallpaperInfo(test_account_id1_, info, true);
  wallpaper_manager->SetUserWallpaperNow(test_account_id1_);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  std::unique_ptr<WallpaperManager::TestApi> test_api;
  test_api.reset(new WallpaperManager::TestApi(wallpaper_manager));
  // Verify SetUserWallpaperNow updates wallpaper cache.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id1_, &cached_wallpaper));
  base::FilePath path;
  EXPECT_TRUE(test_api->GetPathFromCache(test_account_id1_, &path));
  EXPECT_FALSE(path.empty());
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
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTestCacheUpdate,
                       VerifyWallpaperCache) {
  LogIn(test_account_id1_, kTestUser1Hash);

  WallpaperManager* wallpaper_manager = WallpaperManager::Get();

  // Force load initial wallpaper
  // (simulate WallpaperController::UpdateDisplay()).
  wallpaper_manager->UpdateWallpaper(true);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  std::unique_ptr<WallpaperManager::TestApi> test_api;
  test_api.reset(new WallpaperManager::TestApi(wallpaper_manager));
  gfx::ImageSkia cached_wallpaper;
  // Previous custom wallpaper should be cached after user login.
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id1_, &cached_wallpaper));
  base::FilePath original_path;
  EXPECT_TRUE(test_api->GetPathFromCache(test_account_id1_, &original_path));
  EXPECT_FALSE(original_path.empty());

  LogIn(test_account_id2_, kTestUser2Hash);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // Login another user should not delete logged in user's wallpaper cache.
  // Note active user is still test_account_id1_.
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id1_, &cached_wallpaper));
  base::FilePath path;
  EXPECT_TRUE(test_api->GetPathFromCache(test_account_id1_, &path));
  EXPECT_EQ(original_path, path);

  gfx::ImageSkia red_wallpaper = CreateTestImage(SK_ColorRED);
  wallpaper_manager->SetWallpaperFromImageSkia(test_account_id1_, red_wallpaper,
                                               WALLPAPER_LAYOUT_CENTER, true);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // SetWallpaperFromImageSkia should update wallpaper cache when multi-profile
  // is turned on.
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id1_, &cached_wallpaper));
  EXPECT_TRUE(test_api->GetPathFromCache(test_account_id1_, &path));
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(red_wallpaper));

  gfx::ImageSkia green_wallpaper = CreateTestImage(SK_ColorGREEN);
  wallpaper_manager->SetCustomWallpaper(
      test_account_id1_, test_account1_wallpaper_files_id_,
      "dummy",  // dummy file name
      WALLPAPER_LAYOUT_CENTER, user_manager::User::CUSTOMIZED, green_wallpaper,
      true);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // SetCustomWallpaper should also update wallpaper cache when multi-profile is
  // turned on.
  EXPECT_TRUE(
      test_api->GetWallpaperFromCache(test_account_id1_, &cached_wallpaper));
  EXPECT_TRUE(cached_wallpaper.BackedBySameObjectAs(green_wallpaper));
  EXPECT_TRUE(test_api->GetPathFromCache(test_account_id1_, &path));
  EXPECT_NE(original_path, path);

  wallpaper_manager->SetDefaultWallpaperNow(test_account_id1_);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  // SetDefaultWallpaper should invalidate the user's wallpaper cache.
  EXPECT_FALSE(
      test_api->GetWallpaperFromCache(test_account_id1_, &cached_wallpaper));
}

// ----------------------------------------------------------------------
// Test default wallpapers.

class TestObserver : public WallpaperManager::Observer {
 public:
  explicit TestObserver(WallpaperManager* wallpaper_manager)
      : update_wallpaper_count_(0), wallpaper_manager_(wallpaper_manager) {
    DCHECK(wallpaper_manager_);
    wallpaper_manager_->AddObserver(this);
  }

  ~TestObserver() override { wallpaper_manager_->RemoveObserver(this); }

  void OnWallpaperAnimationFinished(const AccountId& account_id) override {}

  void OnUpdateWallpaperForTesting() override { ++update_wallpaper_count_; }

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

#if defined(OS_CHROMEOS) && defined(USE_OZONE)
#define MAYBE_DisplayChange DISABLED_DisplayChange
#else
#define MAYBE_DisplayChange DisplayChange
#endif
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, MAYBE_DisplayChange) {
  TestObserver observer(WallpaperManager::Get());

  // Set the wallpaper to ensure that UpdateWallpaper() will be called when the
  // display configuration changes.
  gfx::ImageSkia image = wallpaper_manager_test_utils::CreateTestImage(
      640, 480, wallpaper_manager_test_utils::kCustomWallpaperColor);
  controller_->SetWallpaperImage(image, WALLPAPER_LAYOUT_STRETCH);

  // Small wallpaper images should be used for configurations less than or
  // equal to kSmallWallpaperMaxWidth by kSmallWallpaperMaxHeight, even if
  // multiple displays are connected.
  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("800x600,800x600");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("1366x800");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // At larger sizes, large wallpapers should be used.
  UpdateDisplay("1367x800");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("1367x801");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("2560x1700");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // Rotated smaller screen may use larger image.
  UpdateDisplay("800x600/r");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("800x600/r,800x600");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_SMALL,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());
  UpdateDisplay("1366x800/r");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(WallpaperManager::WALLPAPER_RESOLUTION_LARGE,
            WallpaperManager::Get()->GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // Max display size didn't change.
  UpdateDisplay("900x800/r,400x1366");
  base::RunLoop().RunUntilIdle();
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());
}

// Test that WallpaperManager loads the appropriate wallpaper
// images as specified via command-line flags in various situations.
// Splitting these into separate tests avoids needing to run animations.
// TODO(derat): Combine these into a single test
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, SmallDefaultWallpaper) {
  CreateCmdlineWallpapers();

  // At 800x600, the small wallpaper should be loaded.
  UpdateDisplay("800x600");
  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kSmallDefaultWallpaperColor));
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, LargeDefaultWallpaper) {
  CreateCmdlineWallpapers();
  UpdateDisplay("1600x1200");
  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kLargeDefaultWallpaperColor));
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       LargeDefaultWallpaperWhenRotated) {
  CreateCmdlineWallpapers();

  UpdateDisplay("1200x800/r");
  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kLargeDefaultWallpaperColor));
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, SmallGuestWallpaper) {
  CreateCmdlineWallpapers();
  SessionManager::Get()->CreateSession(user_manager::GuestAccountId(),
                                       user_manager::kGuestUserName);
  UpdateDisplay("800x600");
  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kSmallGuestWallpaperColor));
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, LargeGuestWallpaper) {
  CreateCmdlineWallpapers();
  SessionManager::Get()->CreateSession(user_manager::GuestAccountId(),
                                       user_manager::kGuestUserName);
  UpdateDisplay("1600x1200");
  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kLargeGuestWallpaperColor));
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, SmallChildWallpaper) {
  CreateCmdlineWallpapers();
  LogInAsChild(test_account_id1_, kTestUser1Hash);
  UpdateDisplay("800x600");
  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kSmallChildWallpaperColor));
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, LargeChildWallpaper) {
  CreateCmdlineWallpapers();
  LogInAsChild(test_account_id1_, kTestUser1Hash);
  UpdateDisplay("1600x1200");
  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();
  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kLargeChildWallpaperColor));
}

IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       SwitchBetweenDefaultAndCustom) {
  // Start loading the default wallpaper.
  UpdateDisplay("640x480");
  CreateCmdlineWallpapers();
  SessionManager::Get()->CreateSession(user_manager::StubAccountId(),
                                       "test_hash");

  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());

  // Custom wallpaper should be applied immediately, canceling the default
  // wallpaper load task.
  gfx::ImageSkia image = wallpaper_manager_test_utils::CreateTestImage(
      640, 480, wallpaper_manager_test_utils::kCustomWallpaperColor);
  WallpaperManager::Get()->SetCustomWallpaper(
      user_manager::StubAccountId(),
      wallpaper::WallpaperFilesId::FromString("test_hash"), "test-nofile.jpeg",
      WALLPAPER_LAYOUT_STRETCH, user_manager::User::CUSTOMIZED, image, true);
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();

  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kCustomWallpaperColor));

  WallpaperManager::Get()->SetDefaultWallpaperNow(EmptyAccountId());
  wallpaper_manager_test_utils::WaitAsyncWallpaperLoadFinished();

  EXPECT_TRUE(wallpaper_manager_test_utils::ImageIsNearColor(
      controller_->GetWallpaper(),
      wallpaper_manager_test_utils::kSmallDefaultWallpaperColor));
}

}  // namespace chromeos

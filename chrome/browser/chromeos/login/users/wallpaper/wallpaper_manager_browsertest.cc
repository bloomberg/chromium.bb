// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"

#include <stddef.h>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
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
#include "components/wallpaper/wallpaper_resizer.h"
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

constexpr char kTestUser1[] = "test1@domain.com";
constexpr char kTestUser1GaiaId[] = "0000000001";
constexpr char kTestUser1Hash[] = "test1@domain.com-hash";
constexpr char kTestUser2[] = "test2@domain.com";
constexpr char kTestUser2GaiaId[] = "0000000002";
constexpr char kTestUser2Hash[] = "test2@domain.com-hash";

}  // namespace

class WallpaperManagerBrowserTest : public InProcessBrowserTest {
 public:
  WallpaperManagerBrowserTest() : controller_(NULL), local_state_(NULL) {}

  ~WallpaperManagerBrowserTest() override {}

  void SetUpOnMainThread() override {
    controller_ = ash::Shell::Get()->wallpaper_controller();
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
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay(display_specs);
  }

  void WaitAsyncWallpaperLoadStarted() { base::RunLoop().RunUntilIdle(); }

 protected:
  // Return custom wallpaper path. Create directory if not exist.
  base::FilePath GetCustomWallpaperPath(
      const char* sub_dir,
      const wallpaper::WallpaperFilesId& wallpaper_files_id,
      const std::string& id) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath wallpaper_path =
        ash::WallpaperController::GetCustomWallpaperPath(
            sub_dir, wallpaper_files_id.id(), id);
    if (!base::DirectoryExists(wallpaper_path.DirName()))
      base::CreateDirectory(wallpaper_path.DirName());

    return wallpaper_path;
  }

  // Logs in |account_id|.
  void LogIn(const AccountId& account_id, const std::string& user_id_hash) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    SessionManager::Get()->CreateSession(account_id, user_id_hash,
                                         false /* is_child */);
    SessionManager::Get()->SessionStarted();
    // Flush to ensure the created session and ACTIVE state reaches ash.
    SessionControllerClient::FlushForTesting();
    WaitAsyncWallpaperLoadStarted();
  }

  // Logs in |account_id| and sets it as child account.
  void LogInAsChild(const AccountId& account_id,
                    const std::string& user_id_hash) {
    SessionManager::Get()->CreateSession(account_id, user_id_hash,
                                         true /* is_child */);
    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);
    CHECK(user->GetType() == user_manager::USER_TYPE_CHILD);
    // TODO(jamescook): For some reason creating the shelf here (which is what
    // would happen in normal login) causes the child wallpaper tests to fail
    // with the wallpaper having alpha. This looks like the wallpaper is mid-
    // animation, but happens even if animations are disabled. Something is
    // wrong with how these tests simulate login.
  }

  ash::WallpaperController* controller_;
  PrefService* local_state_;
  std::unique_ptr<base::CommandLine> wallpaper_manager_command_line_;

  // Directory created by CreateCmdlineWallpapers () to store default
  // wallpaper images.
  std::unique_ptr<base::ScopedTempDir> wallpaper_dir_;

  const AccountId test_account_id1_ =
      AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId);
  const AccountId test_account_id2_ =
      AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId);

  const wallpaper::WallpaperFilesId test_account1_wallpaper_files_id_ =
      wallpaper::WallpaperFilesId::FromString(kTestUser1Hash);
  const wallpaper::WallpaperFilesId test_account2_wallpaper_files_id_ =
      wallpaper::WallpaperFilesId::FromString(kTestUser2Hash);

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperManagerBrowserTest);
};

// Test for http://crbug.com/265689. When hooked up a large external monitor,
// the default large resolution wallpaper should load.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest,
                       HotPlugInScreenAtGAIALoginScreen) {
  UpdateDisplay("800x600");
  // Set initial wallpaper to the default wallpaper.
  ash::Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();

  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  UpdateDisplay("800x600,2000x2000");
}

class TestObserver : public WallpaperManager::Observer {
 public:
  explicit TestObserver(WallpaperManager* wallpaper_manager)
      : update_wallpaper_count_(0), wallpaper_manager_(wallpaper_manager) {
    DCHECK(wallpaper_manager_);
    wallpaper_manager_->AddObserver(this);
  }

  ~TestObserver() override { wallpaper_manager_->RemoveObserver(this); }

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

// TODO: test is flaky. http://crbug.com/691548.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, DISABLED_DisplayChange) {
  TestObserver observer(WallpaperManager::Get());

  // Set the wallpaper to ensure that UpdateWallpaper() will be called when the
  // display configuration changes.
  gfx::ImageSkia image = wallpaper_manager_test_utils::CreateTestImage(
      640, 480, wallpaper_manager_test_utils::kSmallCustomWallpaperColor);
  WallpaperInfo info("", WALLPAPER_LAYOUT_STRETCH, wallpaper::DEFAULT,
                     base::Time::Now().LocalMidnight());
  controller_->SetWallpaperImage(image, info);

  // Small wallpaper images should be used for configurations less than or
  // equal to kSmallWallpaperMaxWidth by kSmallWallpaperMaxHeight, even if
  // multiple displays are connected.
  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("800x600,800x600");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("1366x800");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // At larger sizes, large wallpapers should be used.
  UpdateDisplay("1367x800");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_LARGE,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("1367x801");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_LARGE,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("2560x1700");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_LARGE,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // Rotated smaller screen may use larger image.
  UpdateDisplay("800x600/r");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  UpdateDisplay("800x600/r,800x600");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());
  UpdateDisplay("1366x800/r");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::WallpaperController::WALLPAPER_RESOLUTION_LARGE,
            ash::WallpaperController::GetAppropriateResolution());
  EXPECT_EQ(1, observer.GetUpdateWallpaperCountAndReset());

  // Max display size didn't change.
  UpdateDisplay("900x800/r,400x1366");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer.GetUpdateWallpaperCountAndReset());
}

}  // namespace chromeos

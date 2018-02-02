// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller.h"

#include <cmath>
#include <cstdlib>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/task_scheduler.h"
#include "chromeos/chromeos_switches.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

using wallpaper::WallpaperLayout;
using wallpaper::WALLPAPER_LAYOUT_CENTER;
using wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
using wallpaper::WALLPAPER_LAYOUT_STRETCH;
using wallpaper::WALLPAPER_LAYOUT_TILE;

using session_manager::SessionState;

namespace ash {
namespace {

// Containers IDs used for tests.
const int kWallpaperId = ash::kShellWindowId_WallpaperContainer;
const int kLockScreenWallpaperId =
    ash::kShellWindowId_LockScreenWallpaperContainer;

constexpr int kLargeWallpaperWidth = 256;
const int kLargeWallpaperHeight = WallpaperController::kLargeWallpaperMaxHeight;
constexpr int kSmallWallpaperWidth = 256;
const int kSmallWallpaperHeight = WallpaperController::kSmallWallpaperMaxHeight;

constexpr char default_small_wallpaper_name[] = "small.jpg";
constexpr char default_large_wallpaper_name[] = "large.jpg";
constexpr char guest_small_wallpaper_name[] = "guest_small.jpg";
constexpr char guest_large_wallpaper_name[] = "guest_large.jpg";
constexpr char child_small_wallpaper_name[] = "child_small.jpg";
constexpr char child_large_wallpaper_name[] = "child_large.jpg";

// Colors used to distinguish between wallpapers with large and small
// resolution.
const SkColor kLargeCustomWallpaperColor = SK_ColorDKGRAY;
const SkColor kSmallCustomWallpaperColor = SK_ColorLTGRAY;

// A color that can be passed to |CreateImage|. Specifically chosen to not
// conflict with any of the custom wallpaper colors.
const SkColor kWallpaperColor = SK_ColorMAGENTA;

std::string GetDummyFileId(const AccountId& account_id) {
  return account_id.GetUserEmail() + "-hash";
}

std::string GetDummyFileName(const AccountId& account_id) {
  return account_id.GetUserEmail() + "-file";
}

// Dummy account ids and file ids.
constexpr char user_1[] = "user1@test.com";
const AccountId account_id_1 = AccountId::FromUserEmail(user_1);
const std::string wallpaper_files_id_1 = GetDummyFileId(account_id_1);
const std::string file_name_1 = GetDummyFileName(account_id_1);

constexpr char user_2[] = "user2@test.com";
const AccountId account_id_2 = AccountId::FromUserEmail(user_2);
const std::string wallpaper_files_id_2 = GetDummyFileId(account_id_2);
const std::string file_name_2 = GetDummyFileName(account_id_2);

// Creates an image of size |size|.
gfx::ImageSkia CreateImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

// Returns number of child windows in a shell window container.
int ChildCountForContainer(int container_id) {
  aura::Window* root = Shell::Get()->GetPrimaryRootWindow();
  aura::Window* container = root->GetChildById(container_id);
  return static_cast<int>(container->children().size());
}

// Steps a widget's layer animation until it is completed. Animations must be
// enabled.
void RunAnimationForWidget(views::Widget* widget) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::Layer* layer = widget->GetLayer();
  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    layer->GetAnimator()->Step(step_time +
                               base::TimeDelta::FromMilliseconds(1000));
  }
}

// Returns custom wallpaper path. Creates the directory if it doesn't exist.
base::FilePath GetCustomWallpaperPath(const char* sub_dir,
                                      const std::string& wallpaper_files_id,
                                      const std::string& file_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath wallpaper_path = WallpaperController::GetCustomWallpaperPath(
      sub_dir, wallpaper_files_id, file_name);
  if (!base::DirectoryExists(wallpaper_path.DirName()))
    base::CreateDirectory(wallpaper_path.DirName());

  return wallpaper_path;
}

void WaitUntilCustomWallpapersDeleted(const AccountId& account_id) {
  const std::string wallpaper_file_id = GetDummyFileId(account_id);

  base::FilePath small_wallpaper_dir =
      WallpaperController::GetCustomWallpaperDir(
          WallpaperController::kSmallWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath large_wallpaper_dir =
      WallpaperController::GetCustomWallpaperDir(
          WallpaperController::kLargeWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath original_wallpaper_dir =
      WallpaperController::GetCustomWallpaperDir(
          WallpaperController::kOriginalWallpaperSubDir)
          .Append(wallpaper_file_id);
  base::FilePath thumbnail_wallpaper_dir =
      WallpaperController::GetCustomWallpaperDir(
          WallpaperController::kThumbnailWallpaperSubDir)
          .Append(wallpaper_file_id);

  while (base::PathExists(small_wallpaper_dir) ||
         base::PathExists(large_wallpaper_dir) ||
         base::PathExists(original_wallpaper_dir) ||
         base::PathExists(thumbnail_wallpaper_dir)) {
  }
}

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}
  ~TaskObserver() override = default;

  // MessageLoop::TaskObserver:
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    processed_ = true;
  }

  // Returns true if any task was processed.
  bool processed() const { return processed_; }

 private:
  bool processed_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

// See content::RunAllTasksUntilIdle().
void RunAllTasksUntilIdle() {
  while (true) {
    TaskObserver task_observer;
    base::MessageLoop::current()->AddTaskObserver(&task_observer);
    // May spin message loop.
    base::TaskScheduler::GetInstance()->FlushForTesting();

    base::RunLoop().RunUntilIdle();
    base::MessageLoop::current()->RemoveTaskObserver(&task_observer);

    if (!task_observer.processed())
      break;
  }
}

// A test implementation of the WallpaperObserver mojo interface.
class TestWallpaperObserver : public mojom::WallpaperObserver {
 public:
  TestWallpaperObserver() = default;
  ~TestWallpaperObserver() override = default;

  // mojom::WallpaperObserver:
  void OnWallpaperColorsChanged(
      const std::vector<SkColor>& prominent_colors) override {
    ++wallpaper_colors_changed_count_;
    if (run_loop_)
      run_loop_->Quit();
  }

  int wallpaper_colors_changed_count() const {
    return wallpaper_colors_changed_count_;
  }

  void set_run_loop(base::RunLoop* loop) { run_loop_ = loop; }

 private:
  base::RunLoop* run_loop_ = nullptr;
  int wallpaper_colors_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperObserver);
};

class TestWallpaperControllerObserver : public WallpaperControllerObserver {
 public:
  TestWallpaperControllerObserver() = default;

  void OnWallpaperDataChanged() override {}

  void OnWallpaperBlurChanged() override { ++wallpaper_blur_changed_count_; }

  void Reset() { wallpaper_blur_changed_count_ = 0; }

  int wallpaper_blur_changed_count_ = 0;
};

}  // namespace

class WallpaperControllerTest : public AshTestBase {
 public:
  WallpaperControllerTest() : controller_(nullptr) {}
  ~WallpaperControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    // Ash shell initialization creates wallpaper. Reset it so we can manually
    // control wallpaper creation and animation in our tests.
    RootWindowController* root_window_controller =
        Shell::Get()->GetPrimaryRootWindowController();
    root_window_controller->SetWallpaperWidgetController(nullptr);
    root_window_controller->SetAnimatingWallpaperWidgetController(nullptr);
    controller_ = Shell::Get()->wallpaper_controller();
    controller_->set_wallpaper_reload_delay_for_test(0);
    controller_->InitializePathsForTesting();
  }

  void TearDown() override {
    // Remove the custom wallpaper directories that may be created.
    controller_->RemoveUserWallpaper(InitializeUser(account_id_1),
                                     wallpaper_files_id_1);
    controller_->RemoveUserWallpaper(InitializeUser(account_id_2),
                                     wallpaper_files_id_2);
    AshTestBase::TearDown();
  }

  WallpaperView* wallpaper_view() {
    WallpaperWidgetController* controller =
        Shell::Get()
            ->GetPrimaryRootWindowController()
            ->animating_wallpaper_widget_controller()
            ->GetController(false);
    EXPECT_TRUE(controller);
    return static_cast<WallpaperView*>(
        controller->widget()->GetContentsView()->child_at(0));
  }

 protected:
  // Helper function that tests the wallpaper is always fitted to the native
  // display resolution when the layout is WALLPAPER_LAYOUT_CENTER.
  void WallpaperFitToNativeResolution(WallpaperView* view,
                                      float device_scale_factor,
                                      int image_width,
                                      int image_height,
                                      SkColor color) {
    gfx::Size size = view->bounds().size();
    gfx::Canvas canvas(size, device_scale_factor, true);
    view->OnPaint(&canvas);

    SkBitmap bitmap = canvas.GetBitmap();
    int bitmap_width = bitmap.width();
    int bitmap_height = bitmap.height();
    for (int i = 0; i < bitmap_width; i++) {
      for (int j = 0; j < bitmap_height; j++) {
        if (i >= (bitmap_width - image_width) / 2 &&
            i < (bitmap_width + image_width) / 2 &&
            j >= (bitmap_height - image_height) / 2 &&
            j < (bitmap_height + image_height) / 2) {
          EXPECT_EQ(color, bitmap.getColor(i, j));
        } else {
          EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(i, j));
        }
      }
    }
  }

  // Runs AnimatingWallpaperWidgetController's animation to completion.
  // TODO(bshe): Don't require tests to run animations; it's slow.
  void RunDesktopControllerAnimation() {
    WallpaperWidgetController* controller =
        Shell::Get()
            ->GetPrimaryRootWindowController()
            ->animating_wallpaper_widget_controller()
            ->GetController(false);
    EXPECT_TRUE(controller);
    ASSERT_NO_FATAL_FAILURE(RunAnimationForWidget(controller->widget()));
  }

  // Convenience function to ensure ShouldCalculateColors() returns true.
  void EnableShelfColoring() {
    const gfx::ImageSkia kImage = CreateImage(10, 10, kWallpaperColor);
    controller_->SetWallpaperImage(
        kImage, CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH));
    SetSessionState(SessionState::ACTIVE);

    EXPECT_TRUE(ShouldCalculateColors());
  }

  // Convenience function to set the SessionState.
  void SetSessionState(SessionState session_state) {
    GetSessionControllerClient()->SetSessionState(session_state);
  }

  // Helper function to create a |WallpaperInfo| struct with dummy values
  // given the desired layout.
  wallpaper::WallpaperInfo CreateWallpaperInfo(WallpaperLayout layout) {
    return wallpaper::WallpaperInfo("", layout, wallpaper::DEFAULT,
                                    base::Time::Now().LocalMidnight());
  }

  // Helper function to create a new |mojom::WallpaperUserInfoPtr| instance with
  // default values. In addition, clear the wallpaper count and the decoding
  // request list. May be called multiple times for the same |account_id|.
  mojom::WallpaperUserInfoPtr InitializeUser(const AccountId& account_id) {
    mojom::WallpaperUserInfoPtr wallpaper_user_info =
        mojom::WallpaperUserInfo::New();
    wallpaper_user_info->account_id = account_id;
    wallpaper_user_info->type = user_manager::USER_TYPE_REGULAR;
    wallpaper_user_info->is_ephemeral = false;
    wallpaper_user_info->has_gaia_account = true;
    ClearWallpaperCount();
    ClearDecodeFilePaths();

    return wallpaper_user_info;
  }

  // Saves images with different resolution to corresponding paths and saves
  // wallpaper info to local state, so that subsequent calls of |ShowWallpaper|
  // can retrieve the images and info.
  void CreateAndSaveWallpapers(const AccountId& account_id) {
    std::string wallpaper_files_id = GetDummyFileId(account_id);
    std::string file_name = GetDummyFileName(account_id);
    base::FilePath small_wallpaper_path =
        GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                               wallpaper_files_id, file_name);
    base::FilePath large_wallpaper_path =
        GetCustomWallpaperPath(WallpaperController::kLargeWallpaperSubDir,
                               wallpaper_files_id, file_name);

    // Saves the small/large resolution wallpapers to small/large custom
    // wallpaper paths.
    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        small_wallpaper_path, kSmallWallpaperWidth, kSmallWallpaperHeight,
        kSmallCustomWallpaperColor));
    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        large_wallpaper_path, kLargeWallpaperWidth, kLargeWallpaperHeight,
        kLargeCustomWallpaperColor));

    std::string relative_path =
        base::FilePath(wallpaper_files_id).Append(file_name).value();
    // Saves wallpaper info to local state for user.
    wallpaper::WallpaperInfo info = {
        relative_path, WALLPAPER_LAYOUT_CENTER_CROPPED, wallpaper::CUSTOMIZED,
        base::Time::Now().LocalMidnight()};
    controller_->SetUserWallpaperInfo(account_id, info,
                                      true /*is_persistent=*/);
  }

  // Simulates setting a custom wallpaper by directly setting the wallpaper
  // info.
  void SimulateSettingCustomWallpaper(const AccountId& account_id) {
    controller_->SetUserWallpaperInfo(
        account_id,
        wallpaper::WallpaperInfo("dummy_file_location", WALLPAPER_LAYOUT_CENTER,
                                 wallpaper::CUSTOMIZED,
                                 base::Time::Now().LocalMidnight()),
        true /*is_persistent=*/);
  }

  // Initializes default wallpaper paths "*default_*file" and writes JPEG
  // wallpaper images to them. Only needs to be called (once) by tests that
  // want to test loading of default wallpapers.
  void CreateDefaultWallpapers() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    wallpaper_dir_.reset(new base::ScopedTempDir);
    ASSERT_TRUE(wallpaper_dir_->CreateUniqueTempDir());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const base::FilePath small_file = wallpaper_dir_->GetPath().Append(
        FILE_PATH_LITERAL(default_small_wallpaper_name));
    command_line->AppendSwitchASCII(chromeos::switches::kDefaultWallpaperSmall,
                                    small_file.value());
    const base::FilePath large_file = wallpaper_dir_->GetPath().Append(
        FILE_PATH_LITERAL(default_large_wallpaper_name));
    command_line->AppendSwitchASCII(chromeos::switches::kDefaultWallpaperLarge,
                                    large_file.value());

    const base::FilePath guest_small_file = wallpaper_dir_->GetPath().Append(
        FILE_PATH_LITERAL(guest_small_wallpaper_name));
    command_line->AppendSwitchASCII(chromeos::switches::kGuestWallpaperSmall,
                                    guest_small_file.value());
    const base::FilePath guest_large_file = wallpaper_dir_->GetPath().Append(
        FILE_PATH_LITERAL(guest_large_wallpaper_name));
    command_line->AppendSwitchASCII(chromeos::switches::kGuestWallpaperLarge,
                                    guest_large_file.value());

    const base::FilePath child_small_file = wallpaper_dir_->GetPath().Append(
        FILE_PATH_LITERAL(child_small_wallpaper_name));
    command_line->AppendSwitchASCII(chromeos::switches::kChildWallpaperSmall,
                                    child_small_file.value());
    const base::FilePath child_large_file = wallpaper_dir_->GetPath().Append(
        FILE_PATH_LITERAL(child_large_wallpaper_name));
    command_line->AppendSwitchASCII(chromeos::switches::kChildWallpaperLarge,
                                    child_large_file.value());

    const int kWallpaperSize = 2;
    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        small_file, kWallpaperSize, kWallpaperSize, kWallpaperColor));
    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        large_file, kWallpaperSize, kWallpaperSize, kWallpaperColor));

    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        guest_small_file, kWallpaperSize, kWallpaperSize, kWallpaperColor));
    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        guest_large_file, kWallpaperSize, kWallpaperSize, kWallpaperColor));

    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        child_small_file, kWallpaperSize, kWallpaperSize, kWallpaperColor));
    ASSERT_TRUE(WallpaperController::WriteJPEGFileForTesting(
        child_large_file, kWallpaperSize, kWallpaperSize, kWallpaperColor));
  }

  // Wrapper for private ShouldCalculateColors().
  bool ShouldCalculateColors() { return controller_->ShouldCalculateColors(); }

  // Wrapper for private IsActiveUserWallpaperControlledByPolicyImpl().
  bool IsActiveUserWallpaperControlledByPolicy() {
    return controller_->IsActiveUserWallpaperControlledByPolicyImpl();
  }

  int GetWallpaperCount() { return controller_->wallpaper_count_for_testing_; }

  void SetBypassDecode() { controller_->bypass_decode_for_testing_ = true; }

  void ClearWallpaperCount() { controller_->wallpaper_count_for_testing_ = 0; }

  void ClearDecodeFilePaths() {
    controller_->decode_requests_for_testing_.clear();
  }

  bool CompareDecodeFilePaths(const std::vector<base::FilePath> expected) {
    if (controller_->decode_requests_for_testing_.size() != expected.size())
      return false;

    for (size_t i = 0; i < expected.size(); ++i) {
      if (controller_->decode_requests_for_testing_[i] != expected[i])
        return false;
    }
    return true;
  }

  WallpaperController* controller_;  // Not owned.

  // Directory created by |CreateDefaultWallpapers| to store default wallpaper
  // images.
  std::unique_ptr<base::ScopedTempDir> wallpaper_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerTest);
};

TEST_F(WallpaperControllerTest, BasicReparenting) {
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaper();

  // Wallpaper view/window exists in the wallpaper container and nothing is in
  // the lock screen wallpaper container.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));

  // Moving wallpaper to lock container should succeed the first time but
  // subsequent calls should do nothing.
  EXPECT_TRUE(controller->MoveToLockedContainer());
  EXPECT_FALSE(controller->MoveToLockedContainer());

  // One window is moved from desktop to lock container.
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(1, ChildCountForContainer(kLockScreenWallpaperId));

  // Moving wallpaper to desktop container should succeed the first time.
  EXPECT_TRUE(controller->MoveToUnlockedContainer());
  EXPECT_FALSE(controller->MoveToUnlockedContainer());

  // One window is moved from lock to desktop container.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));
}

TEST_F(WallpaperControllerTest, ControllerOwnership) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create the wallpaper and its view.
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaper();

  // The new wallpaper is ready to animate.
  RootWindowController* root_window_controller =
      Shell::Get()->GetPrimaryRootWindowController();
  EXPECT_TRUE(root_window_controller->animating_wallpaper_widget_controller()
                  ->GetController(false));
  EXPECT_FALSE(root_window_controller->wallpaper_widget_controller());

  // Force the animation to play to completion.
  RunDesktopControllerAnimation();
  EXPECT_FALSE(root_window_controller->animating_wallpaper_widget_controller()
                   ->GetController(false));
  EXPECT_TRUE(root_window_controller->wallpaper_widget_controller());
}

// Test for crbug.com/149043 "Unlock screen, no launcher appears". Ensure we
// move all wallpaper views if there are more than one.
TEST_F(WallpaperControllerTest, WallpaperMovementDuringUnlock) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaper();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // User locks the screen, which moves the wallpaper forward.
  controller->MoveToLockedContainer();

  // Suspend/resume cycle causes wallpaper to refresh, loading a new wallpaper
  // that will animate in on top of the old one.
  controller->CreateEmptyWallpaper();

  // In this state we have two wallpaper views stored in different properties.
  // Both are in the lock screen wallpaper container.
  RootWindowController* root_window_controller =
      Shell::Get()->GetPrimaryRootWindowController();
  EXPECT_TRUE(root_window_controller->animating_wallpaper_widget_controller()
                  ->GetController(false));
  EXPECT_TRUE(root_window_controller->wallpaper_widget_controller());
  EXPECT_EQ(0, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(2, ChildCountForContainer(kLockScreenWallpaperId));

  // Before the wallpaper's animation completes, user unlocks the screen, which
  // moves the wallpaper to the back.
  controller->MoveToUnlockedContainer();

  // Ensure both wallpapers have moved.
  EXPECT_EQ(2, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));

  // Finish the new wallpaper animation.
  RunDesktopControllerAnimation();

  // Now there is one wallpaper, in the back.
  EXPECT_EQ(1, ChildCountForContainer(kWallpaperId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenWallpaperId));
}

// Test for crbug.com/156542. Animating wallpaper should immediately finish
// animation and replace current wallpaper before next animation starts.
TEST_F(WallpaperControllerTest, ChangeWallpaperQuick) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  WallpaperController* controller = Shell::Get()->wallpaper_controller();
  controller->CreateEmptyWallpaper();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // Change to a new wallpaper.
  controller->CreateEmptyWallpaper();

  RootWindowController* root_window_controller =
      Shell::Get()->GetPrimaryRootWindowController();
  WallpaperWidgetController* animating_controller =
      root_window_controller->animating_wallpaper_widget_controller()
          ->GetController(false);
  EXPECT_TRUE(animating_controller);
  EXPECT_TRUE(root_window_controller->wallpaper_widget_controller());

  // Change to another wallpaper before animation finished.
  controller->CreateEmptyWallpaper();

  // The animating controller should immediately move to wallpaper controller.
  EXPECT_EQ(animating_controller,
            root_window_controller->wallpaper_widget_controller());

  // Cache the new animating controller.
  animating_controller =
      root_window_controller->animating_wallpaper_widget_controller()
          ->GetController(false);

  // Run wallpaper show animation to completion.
  ASSERT_NO_FATAL_FAILURE(RunAnimationForWidget(
      root_window_controller->animating_wallpaper_widget_controller()
          ->GetController(false)
          ->widget()));

  EXPECT_TRUE(root_window_controller->wallpaper_widget_controller());
  EXPECT_FALSE(root_window_controller->animating_wallpaper_widget_controller()
                   ->GetController(false));
  // The wallpaper controller should be the last created animating controller.
  EXPECT_EQ(animating_controller,
            root_window_controller->wallpaper_widget_controller());
}

TEST_F(WallpaperControllerTest, ResizeCustomWallpaper) {
  UpdateDisplay("320x200");

  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);

  // Set the image as custom wallpaper, wait for the resize to finish, and check
  // that the resized image is the expected size.
  controller_->SetWallpaperImage(image,
                                 CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH));
  EXPECT_TRUE(image.BackedBySameObjectAs(controller_->GetWallpaper()));
  RunAllTasksUntilIdle();
  gfx::ImageSkia resized_image = controller_->GetWallpaper();
  EXPECT_FALSE(image.BackedBySameObjectAs(resized_image));
  EXPECT_EQ(gfx::Size(320, 200).ToString(), resized_image.size().ToString());

  // Load the original wallpaper again and check that we're still using the
  // previously-resized image instead of doing another resize
  // (http://crbug.com/321402).
  controller_->SetWallpaperImage(image,
                                 CreateWallpaperInfo(WALLPAPER_LAYOUT_STRETCH));
  RunAllTasksUntilIdle();
  EXPECT_TRUE(resized_image.BackedBySameObjectAs(controller_->GetWallpaper()));
}

TEST_F(WallpaperControllerTest, GetMaxDisplaySize) {
  // Device scale factor shouldn't affect the native size.
  UpdateDisplay("1000x300*2");
  EXPECT_EQ("1000x300",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // Rotated display should return the rotated size.
  UpdateDisplay("1000x300*2/r");
  EXPECT_EQ("300x1000",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // UI Scaling shouldn't affect the native size.
  UpdateDisplay("1000x300*2@1.5");
  EXPECT_EQ("1000x300",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // First display has maximum size.
  UpdateDisplay("400x300,100x100");
  EXPECT_EQ("400x300",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // Second display has maximum size.
  UpdateDisplay("400x300,500x600");
  EXPECT_EQ("500x600",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());

  // Maximum width and height belongs to different displays.
  UpdateDisplay("400x300,100x500");
  EXPECT_EQ("400x500",
            WallpaperController::GetMaxDisplaySizeInNative().ToString());
}

// Test that the wallpaper is always fitted to the native display resolution
// when the layout is WALLPAPER_LAYOUT_CENTER to prevent blurry images.
TEST_F(WallpaperControllerTest, DontScaleWallpaperWithCenterLayout) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Size high_resolution(3600, 2400);
  const gfx::Size low_resolution(360, 240);
  const float high_dsf = 2.0f;
  const float low_dsf = 1.0f;

  gfx::ImageSkia image_high_res = CreateImage(
      high_resolution.width(), high_resolution.height(), kWallpaperColor);
  gfx::ImageSkia image_low_res = CreateImage(
      low_resolution.width(), low_resolution.height(), kWallpaperColor);

  UpdateDisplay("1200x600*2");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 high resolution"));
    controller_->SetWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER));
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 low resolution"));
    controller_->SetWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER));
    WallpaperFitToNativeResolution(wallpaper_view(), high_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 high resolution"));
    controller_->SetWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER));
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 low resolution"));
    controller_->SetWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER));
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }

  UpdateDisplay("1200x600/u@1.5");  // 1.5 ui scale
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 high resolution"));
    controller_->SetWallpaperImage(
        image_high_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER));
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   high_resolution.width(),
                                   high_resolution.height(), kWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 low resolution"));
    controller_->SetWallpaperImage(
        image_low_res, CreateWallpaperInfo(WALLPAPER_LAYOUT_CENTER));
    WallpaperFitToNativeResolution(wallpaper_view(), low_dsf,
                                   low_resolution.width(),
                                   low_resolution.height(), kWallpaperColor);
  }
}

TEST_F(WallpaperControllerTest, ShouldCalculateColorsBasedOnImage) {
  EnableShelfColoring();
  EXPECT_TRUE(ShouldCalculateColors());

  controller_->CreateEmptyWallpaper();
  EXPECT_FALSE(ShouldCalculateColors());
}

TEST_F(WallpaperControllerTest, ShouldCalculateColorsBasedOnSessionState) {
  EnableShelfColoring();

  SetSessionState(SessionState::UNKNOWN);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::OOBE);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(ShouldCalculateColors());

  SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(ShouldCalculateColors());

  SetSessionState(SessionState::LOGIN_SECONDARY);
  EXPECT_FALSE(ShouldCalculateColors());
}

TEST_F(WallpaperControllerTest, MojoWallpaperObserverTest) {
  TestWallpaperObserver observer;
  mojom::WallpaperObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::WallpaperObserver> binding(
      &observer, mojo::MakeRequestAssociatedWithDedicatedPipe(&observer_ptr));
  controller_->AddObserver(observer_ptr.PassInterface());
  controller_->FlushForTesting();

  // Adding an observer fires OnWallpaperColorsChanged() immediately.
  EXPECT_EQ(1, observer.wallpaper_colors_changed_count());

  // Enable shelf coloring will set a customized wallpaper image and change
  // session state to ACTIVE, which will trigger wallpaper colors calculation.
  base::RunLoop run_loop;
  observer.set_run_loop(&run_loop);
  EnableShelfColoring();
  // Color calculation may be asynchronous.
  run_loop.Run();
  // Mojo methods are called after color calculation finishes.
  controller_->FlushForTesting();
  EXPECT_EQ(2, observer.wallpaper_colors_changed_count());
}

TEST_F(WallpaperControllerTest, SetCustomWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(user_1);

  // Verify the wallpaper is set successfully and wallpaper info is updated.
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::CUSTOMIZED);
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /* is_persistent */));
  wallpaper::WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      wallpaper::CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Verify that the wallpaper is not set when |show_wallpaper| is false, but
  // wallpaper info is updated properly.
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetOnlineWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  const std::string url = "dummy_url";
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(user_1);

  // Verify the wallpaper is set successfully and wallpaper info is updated.
  controller_->SetOnlineWallpaper(InitializeUser(account_id_1), *image.bitmap(),
                                  url, layout, true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::ONLINE);
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo expected_wallpaper_info(
      url, layout, wallpaper::ONLINE, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Verify that the wallpaper is not set when |show_wallpaper| is false, but
  // wallpaper info is updated properly.
  controller_->SetOnlineWallpaper(InitializeUser(account_id_1), *image.bitmap(),
                                  url, layout, false /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetAndRemovePolicyWallpaper) {
  SetBypassDecode();
  // Simulate the login screen.
  ClearLogin();

  // The user starts with no wallpaper info and is not controlled by policy.
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 true /*is_persistent=*/));
  EXPECT_FALSE(
      controller_->IsPolicyControlled(account_id_1, true /*is_persistent=*/));
  // A default wallpaper is shown for the user.
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);

  // Set a policy wallpaper. Verify that the user becomes policy controlled and
  // the wallpaper info is updated.
  controller_->SetPolicyWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo policy_wallpaper_info(
      base::FilePath(wallpaper_files_id_1)
          .Append("policy-controlled.jpeg")
          .value(),
      wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED, wallpaper::POLICY,
      base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id_1, true /*is_persistent=*/));
  // Verify the wallpaper is not updated since the user hasn't logged in.
  EXPECT_EQ(0, GetWallpaperCount());

  // Log in the user. Verify the policy wallpaper is now being shown.
  SimulateUserLogin(user_1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::POLICY);

  // Log out the user and remove the policy wallpaper. Verify the wallpaper
  // info is reset to default and the user is no longer policy controlled.
  ClearLogin();
  controller_->RemovePolicyWallpaper(InitializeUser(account_id_1),
                                     wallpaper_files_id_1);
  WaitUntilCustomWallpapersDeleted(account_id_1);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo default_wallpaper_info(
      std::string(), wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
      wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  EXPECT_FALSE(
      controller_->IsPolicyControlled(account_id_1, true /*is_persistent=*/));
  // Verify the wallpaper is not updated since the user hasn't logged in.
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::POLICY);

  // Log in the user. Verify the default wallpaper is now being shown.
  SimulateUserLogin(user_1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForRegularAccount) {
  CreateDefaultWallpapers();
  SimulateUserLogin(user_1);

  // First, simulate setting a user custom wallpaper.
  SimulateSettingCustomWallpaper(account_id_1);
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo default_wallpaper_info(
      std::string(), wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
      wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the large default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(default_large_wallpaper_name)}));

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id_1);
  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the small default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(default_small_wallpaper_name)}));

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id_1);
  // Verify that when screen is rotated, |SetDefaultWallpaper| removes the
  // previously set custom wallpaper info, and the small default wallpaper is
  // set successfully with the correct file path.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(default_small_wallpaper_name)}));

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForChildAccount) {
  CreateDefaultWallpapers();

  const std::string child_email = "child@test.com";
  const AccountId child_account_id = AccountId::FromUserEmail(child_email);
  const std::string child_wallpaper_files_id = GetDummyFileId(child_account_id);
  SimulateUserLogin(child_email);

  // Verify the large child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  mojom::WallpaperUserInfoPtr wallpaper_user_info =
      InitializeUser(child_account_id);
  wallpaper_user_info->type = user_manager::USER_TYPE_CHILD;
  controller_->SetDefaultWallpaper(std::move(wallpaper_user_info),
                                   child_wallpaper_files_id,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(child_large_wallpaper_name)}));

  // Verify the small child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  wallpaper_user_info = InitializeUser(child_account_id);
  wallpaper_user_info->type = user_manager::USER_TYPE_CHILD;
  controller_->SetDefaultWallpaper(std::move(wallpaper_user_info),
                                   child_wallpaper_files_id,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(child_small_wallpaper_name)}));
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForGuestSession) {
  CreateDefaultWallpapers();

  // First, simulate setting a custom wallpaper for a regular user.
  SimulateUserLogin(user_1);
  SimulateSettingCustomWallpaper(account_id_1);
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo default_wallpaper_info(
      std::string(), wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
      wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  SimulateGuestLogin();

  // Verify that during a guest session, |SetDefaultWallpaper| removes the user
  // custom wallpaper info, but a guest specific wallpaper should be set,
  // instead of the regular default wallpaper.
  UpdateDisplay("1600x1200");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(guest_large_wallpaper_name)}));

  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(guest_small_wallpaper_name)}));
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestInKioskMode) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  const std::string kiosk_app = "kiosk";

  // Simulate kiosk login.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(kiosk_app, user_manager::USER_TYPE_KIOSK_APP);
  session->SwitchActiveUser(AccountId::FromUserEmail(kiosk_app));
  session->SetSessionState(SessionState::ACTIVE);

  // Verify that |SetCustomWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 true /* is_persistent */));

  // Verify that |SetOnlineWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetOnlineWallpaper(InitializeUser(account_id_1), *image.bitmap(),
                                  "dummy_url", WALLPAPER_LAYOUT_CENTER,
                                  true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 true /*is_persistent=*/));

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                 true /*is_persistent=*/));
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestWhenPolicyIsEnforced) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(user_1);

  // Simulate setting a policy wallpaper by setting the wallpaper info.
  // TODO(crbug.com/776464): Replace this with a real |SetPolicyWallpaper| call.
  wallpaper::WallpaperInfo policy_wallpaper_info(
      "dummy_file_location", WALLPAPER_LAYOUT_CENTER, wallpaper::POLICY,
      base::Time::Now().LocalMidnight());
  controller_->SetUserWallpaperInfo(account_id_1, policy_wallpaper_info,
                                    true /*is_persistent=*/);
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id_1, true /*is_persistent=*/));

  // Verify that |SetCustomWallpaper| doesn't set wallpaper when policy is
  // enforced, and |account_id|'s wallpaper info is not updated.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetOnlineWallpaper| doesn't set wallpaper when policy is
  // enforced, and |account_id|'s wallpaper info is not updated.
  controller_->SetOnlineWallpaper(InitializeUser(account_id_1), *image.bitmap(),
                                  "dummy_url", WALLPAPER_LAYOUT_CENTER,
                                  true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper when policy is
  // enforced, and |account_id|'s wallpaper info is not updated.
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);
}

TEST_F(WallpaperControllerTest, VerifyWallpaperCache) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  SimulateUserLogin(user_1);

  // |user_1| doesn't have wallpaper cache in the beginning.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  base::FilePath path;
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |SetOnlineWallpaper| updates wallpaper cache for |user1|.
  controller_->SetOnlineWallpaper(
      InitializeUser(account_id_1), *image.bitmap(), "dummy_file_location",
      WALLPAPER_LAYOUT_CENTER, true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // After |user_2| is logged in, |user1|'s wallpaper cache should still be kept
  // (crbug.com/339576). Note the active user is still |user1|.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(user_2);
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |SetDefaultWallpaper| clears wallpaper cache.
  controller_->SetDefaultWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1,
                                   true /*show_wallpaper=*/);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |SetCustomWallpaper| updates wallpaper cache for |user1|.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id_1), wallpaper_files_id_1, file_name_1,
      WALLPAPER_LAYOUT_CENTER, *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id_1, &path));

  // Verify |RemoveUserWallpaper| clears wallpaper cache.
  controller_->RemoveUserWallpaper(InitializeUser(account_id_1),
                                   wallpaper_files_id_1);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id_1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id_1, &path));
}

// Tests that the appropriate wallpaper (large vs. small) is shown depending
// on the desktop resolution.
TEST_F(WallpaperControllerTest, ShowCustomWallpaperWithCorrectResolution) {
  CreateDefaultWallpapers();
  base::FilePath small_wallpaper_path =
      GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  base::FilePath large_wallpaper_path =
      GetCustomWallpaperPath(WallpaperController::kLargeWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);

  CreateAndSaveWallpapers(account_id_1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  // Display is initialized to 800x600. The small resolution custom wallpaper is
  // expected. A second decode request with small resolution default wallpaper
  // is also expected. (Because unit tests don't support actual wallpaper
  // decoding, it falls back to the default wallpaper.)
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_TRUE(CompareDecodeFilePaths(
      {small_wallpaper_path,
       wallpaper_dir_->GetPath().Append(default_small_wallpaper_name)}));

  // Hook up another 800x600 display. This shouldn't trigger a reload.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,800x600");
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(CompareDecodeFilePaths({}));

  // Detach the secondary display.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  // Hook up a 2000x2000 display. The large resolution custom wallpaper should
  // be loaded.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,2000x2000");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_TRUE(CompareDecodeFilePaths(
      {large_wallpaper_path,
       wallpaper_dir_->GetPath().Append(default_large_wallpaper_name)}));

  // Detach the secondary display.
  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  // Hook up the 2000x2000 display again. The large resolution default wallpaper
  // should persist. Test for crbug/165788.
  ClearWallpaperCount();
  ClearDecodeFilePaths();
  UpdateDisplay("800x600,2000x2000");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_TRUE(CompareDecodeFilePaths(
      {large_wallpaper_path,
       wallpaper_dir_->GetPath().Append(default_large_wallpaper_name)}));
}

// After the display is rotated, the sign in wallpaper should be kept. Test for
// crbug.com/794725.
TEST_F(WallpaperControllerTest, SigninWallpaperIsKeptAfterRotation) {
  CreateDefaultWallpapers();

  UpdateDisplay("800x600");
  RunAllTasksUntilIdle();
  controller_->ShowSigninWallpaper();
  RunAllTasksUntilIdle();
  // Display is initialized to 800x600. The small resolution default wallpaper
  // is expected.
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(default_small_wallpaper_name)}));

  ClearWallpaperCount();
  ClearDecodeFilePaths();
  // After rotating the display, the small resolution default wallpaper should
  // still be expected, instead of a custom wallpaper.
  UpdateDisplay("800x600/r");
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::DEFAULT);
  EXPECT_TRUE(CompareDecodeFilePaths(
      {wallpaper_dir_->GetPath().Append(default_small_wallpaper_name)}));
}

// If clients call |ShowUserWallpaper| twice with the same account id, the
// latter request should be prevented (See crbug.com/158383).
TEST_F(WallpaperControllerTest, PreventReloadingSameWallpaper) {
  CreateAndSaveWallpapers(account_id_1);
  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());

  controller_->ShowUserWallpaper(InitializeUser(account_id_1));
  RunAllTasksUntilIdle();
  // No wallpaper is set for the second |ShowUserWallpaper| request.
  EXPECT_EQ(0, GetWallpaperCount());
}

TEST_F(WallpaperControllerTest, UpdateCustomWallpaperLayout) {
  gfx::ImageSkia image = CreateImage(640, 480, kSmallCustomWallpaperColor);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  SimulateUserLogin(user_1);

  // Set a custom wallpaper for the user. Verify that it's set successfully
  // and the wallpaper info is updated.
  controller_->SetCustomWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1, file_name_1, layout,
                                  *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /* is_persistent */));
  wallpaper::WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id_1).Append(file_name_1).value(), layout,
      wallpaper::CUSTOMIZED, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Now change to a different layout. Verify that the layout is updated for
  // both the current wallpaper and the saved wallpaper info.
  layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
  controller_->UpdateCustomWallpaperLayout(InitializeUser(account_id_1),
                                           layout);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /* is_persistent */));
  expected_wallpaper_info.layout = layout;
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Now set an online wallpaper. Verify that it's set successfully and the
  // wallpaper info is updated.
  const std::string url = "dummy_url";
  image = CreateImage(640, 480, kWallpaperColor);
  controller_->SetOnlineWallpaper(InitializeUser(account_id_1), *image.bitmap(),
                                  url, layout, true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperType(), wallpaper::ONLINE);
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /*is_persistent=*/));
  expected_wallpaper_info.type = wallpaper::ONLINE;
  expected_wallpaper_info.location = url;
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Now change the layout of the online wallpaper. Verify that it's a no-op.
  controller_->UpdateCustomWallpaperLayout(InitializeUser(account_id_1),
                                           WALLPAPER_LAYOUT_CENTER);
  RunAllTasksUntilIdle();
  // The wallpaper is not updated.
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_EQ(controller_->GetWallpaperLayout(), layout);
  // The saved wallpaper info is not updated.
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id_1, &wallpaper_info,
                                                true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);
}

// Tests that if a user who has a custom wallpaper is removed from the device,
// only the directory that contains the user's custom wallpapers gets removed.
// The other user's custom wallpaper is not affected.
TEST_F(WallpaperControllerTest, RemoveUserWithCustomWallpaper) {
  SimulateUserLogin(user_1);
  base::FilePath small_wallpaper_path_1 =
      GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  // Set a custom wallpaper for |user_1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(account_id_1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a custom wallpaper for the user.
  SimulateUserLogin(user_2);
  base::FilePath small_wallpaper_path_2 = GetCustomWallpaperPath(
      WallpaperController::kSmallWallpaperSubDir, wallpaper_files_id_2,
      GetDummyFileName(account_id_2));
  CreateAndSaveWallpapers(account_id_2);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_2));

  // Simulate the removal of |user_2|.
  controller_->RemoveUserWallpaper(InitializeUser(account_id_2),
                                   wallpaper_files_id_2);
  // Wait until all files under the user's custom wallpaper directory are
  // removed.
  WaitUntilCustomWallpapersDeleted(account_id_2);
  EXPECT_FALSE(base::PathExists(small_wallpaper_path_2));

  // Verify that the other user's wallpaper is not affected.
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));
}

// Tests that if a user who has a default wallpaper is removed from the device,
// the other user's custom wallpaper is not affected.
TEST_F(WallpaperControllerTest, RemoveUserWithDefaultWallpaper) {
  SimulateUserLogin(user_1);
  base::FilePath small_wallpaper_path_1 =
      GetCustomWallpaperPath(WallpaperController::kSmallWallpaperSubDir,
                             wallpaper_files_id_1, file_name_1);
  // Set a custom wallpaper for |user_1| and verify the wallpaper exists.
  CreateAndSaveWallpapers(account_id_1);
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));

  // Now login another user and set a default wallpaper for the user.
  SimulateUserLogin(user_2);
  controller_->SetDefaultWallpaper(InitializeUser(account_id_2),
                                   wallpaper_files_id_2,
                                   true /*show_wallpaper=*/);

  // Simulate the removal of |user_2|.
  controller_->RemoveUserWallpaper(InitializeUser(account_id_2),
                                   wallpaper_files_id_2);

  // Verify that the other user's wallpaper is not affected.
  EXPECT_TRUE(base::PathExists(small_wallpaper_path_1));
}

TEST_F(WallpaperControllerTest, IsActiveUserWallpaperControlledByPolicy) {
  SetBypassDecode();
  // Simulate the login screen. Verify that it returns false since there's no
  // active user.
  ClearLogin();
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());

  SimulateUserLogin(user_1);
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());
  // Set a policy wallpaper for the active user. Verify that the active user
  // becomes policy controlled.
  controller_->SetPolicyWallpaper(InitializeUser(account_id_1),
                                  wallpaper_files_id_1,
                                  std::string() /*data=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(IsActiveUserWallpaperControlledByPolicy());

  // Switch the active user. Verify the active user is not policy controlled.
  SimulateUserLogin(user_2);
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());

  // Logs out. Verify that it returns false since there's no active user.
  ClearLogin();
  EXPECT_FALSE(IsActiveUserWallpaperControlledByPolicy());
}

TEST_F(WallpaperControllerTest, WallpaperBlur) {
  ASSERT_TRUE(controller_->IsBlurEnabled());
  ASSERT_FALSE(controller_->IsWallpaperBlurred());

  TestWallpaperControllerObserver observer;
  controller_->AddObserver(&observer);

  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOCKED);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(1, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(2, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(3, observer.wallpaper_blur_changed_count_);

  // Blur state does not change below.
  observer.Reset();
  SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::OOBE);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::UNKNOWN);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  controller_->RemoveObserver(&observer);
}

TEST_F(WallpaperControllerTest, WallpaperBlurDisabledByPolicy) {
  // Simulate DEVICE policy wallpaper.
  const wallpaper::WallpaperInfo info("", WALLPAPER_LAYOUT_CENTER,
                                      wallpaper::DEVICE, base::Time::Now());
  const gfx::ImageSkia image = CreateImage(10, 10, kWallpaperColor);
  controller_->SetWallpaperImage(image, info);
  ASSERT_FALSE(controller_->IsBlurEnabled());
  ASSERT_FALSE(controller_->IsWallpaperBlurred());

  TestWallpaperControllerObserver observer;
  controller_->AddObserver(&observer);

  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOGIN_SECONDARY);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::OOBE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::UNKNOWN);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(0, observer.wallpaper_blur_changed_count_);

  controller_->RemoveObserver(&observer);
}

TEST_F(WallpaperControllerTest, WallpaperBlurDuringLockScreenTransition) {
  ASSERT_TRUE(controller_->IsBlurEnabled());
  ASSERT_FALSE(controller_->IsWallpaperBlurred());

  TestWallpaperControllerObserver observer;
  controller_->AddObserver(&observer);

  // Simulate lock and unlock sequence.
  controller_->PrepareWallpaperForLockScreenChange(true);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(1, observer.wallpaper_blur_changed_count_);

  SetSessionState(SessionState::LOCKED);
  EXPECT_TRUE(controller_->IsWallpaperBlurred());

  // Change of state to ACTIVE trigers post lock animation and
  // PrepareWallpaperForLockScreenChange(false)
  SetSessionState(SessionState::ACTIVE);
  EXPECT_FALSE(controller_->IsWallpaperBlurred());
  EXPECT_EQ(2, observer.wallpaper_blur_changed_count_);

  controller_->RemoveObserver(&observer);
}

}  // namespace ash

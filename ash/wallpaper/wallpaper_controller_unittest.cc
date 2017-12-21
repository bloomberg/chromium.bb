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
#include "ash/wallpaper/test_wallpaper_delegate.h"
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

const std::string default_small_wallpaper_name = "small.jpg";
const std::string default_large_wallpaper_name = "large.jpg";
const std::string guest_small_wallpaper_name = "guest_small.jpg";
const std::string guest_large_wallpaper_name = "guest_large.jpg";
const std::string child_small_wallpaper_name = "child_small.jpg";
const std::string child_large_wallpaper_name = "child_large.jpg";

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

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}
  ~TaskObserver() override = default;

  // MessageLoop::TaskObserver overrides.
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
  WallpaperControllerTest()
      : controller_(nullptr), wallpaper_delegate_(nullptr) {}
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
    wallpaper_delegate_ =
        static_cast<TestWallpaperDelegate*>(Shell::Get()->wallpaper_delegate());
    controller_->set_wallpaper_reload_delay_for_test(0);
    controller_->InitializePathsForTesting();
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
  // A color that can be passed to CreateImage(). Specifically chosen to not
  // conflict with any of the default wallpaper colors.
  static const SkColor kWallpaperColor = SK_ColorMAGENTA;

  // Creates an image of size |size|.
  gfx::ImageSkia CreateImage(int width, int height, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(color);
    gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    return image;
  }

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
  // default values. In addition, clear the wallpaper count. May be called
  // multiple times for the same |account_id|.
  mojom::WallpaperUserInfoPtr InitializeUser(const AccountId& account_id) {
    mojom::WallpaperUserInfoPtr wallpaper_user_info =
        mojom::WallpaperUserInfo::New();
    wallpaper_user_info->account_id = account_id;
    wallpaper_user_info->type = user_manager::USER_TYPE_REGULAR;
    wallpaper_user_info->is_ephemeral = false;
    wallpaper_user_info->has_gaia_account = true;

    controller_->current_user_wallpaper_info_ = wallpaper::WallpaperInfo();
    controller_->wallpaper_count_for_testing_ = 0;

    return wallpaper_user_info;
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

  // Wrapper for private ShouldCalculateColors()
  bool ShouldCalculateColors() { return controller_->ShouldCalculateColors(); }

  int GetWallpaperCount() { return controller_->wallpaper_count_for_testing_; }

  base::FilePath GetWallpaperFilePath() {
    return controller_->wallpaper_file_path_for_testing_;
  }

  WallpaperController* controller_;  // Not owned.

  TestWallpaperDelegate* wallpaper_delegate_;

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
  const std::string file_name = "dummy_file_name";
  const std::string wallpaper_files_id = "dummy_file_id";
  const std::string user_email = "user@test.com";
  const AccountId account_id = AccountId::FromUserEmail(user_email);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;
  wallpaper::WallpaperType type = wallpaper::CUSTOMIZED;

  SimulateUserLogin(user_email);

  // Verify the wallpaper is set successfully and wallpaper info is updated.
  controller_->SetCustomWallpaper(InitializeUser(account_id),
                                  wallpaper_files_id, file_name, layout, type,
                                  *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo expected_wallpaper_info(
      base::FilePath(wallpaper_files_id).Append(file_name).value(), layout,
      type, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Verify that the wallpaper is not set when |show_wallpaper| is false, but
  // wallpaper info is updated properly.
  controller_->SetCustomWallpaper(InitializeUser(account_id),
                                  wallpaper_files_id, file_name, layout, type,
                                  *image.bitmap(), true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);
  // Remove the custom wallpaper directory that may be created.
  controller_->RemoveUserWallpaper(InitializeUser(account_id),
                                   wallpaper_files_id);
}

TEST_F(WallpaperControllerTest, SetOnlineWallpaper) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  const std::string url = "dummy_url";
  const std::string user_email = "user@test.com";
  const AccountId account_id = AccountId::FromUserEmail(user_email);
  WallpaperLayout layout = WALLPAPER_LAYOUT_CENTER;

  SimulateUserLogin(user_email);

  // Verify the wallpaper is set successfully and wallpaper info is updated.
  controller_->SetOnlineWallpaper(InitializeUser(account_id), *image.bitmap(),
                                  url, layout, true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo expected_wallpaper_info(
      url, layout, wallpaper::ONLINE, base::Time::Now().LocalMidnight());
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);

  // Verify that the wallpaper is not set when |show_wallpaper| is false, but
  // wallpaper info is updated properly.
  controller_->SetOnlineWallpaper(InitializeUser(account_id), *image.bitmap(),
                                  url, layout, false /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  EXPECT_EQ(wallpaper_info, expected_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForRegularAccount) {
  CreateDefaultWallpapers();

  const std::string user_email = "user@test.com";
  const AccountId account_id = AccountId::FromUserEmail(user_email);
  SimulateUserLogin(user_email);

  // First, simulate setting a user custom wallpaper.
  SimulateSettingCustomWallpaper(account_id);
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  wallpaper::WallpaperInfo default_wallpaper_info(
      std::string(), wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
      wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the large default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("1600x1200");
  controller_->SetDefaultWallpaper(InitializeUser(account_id),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(wallpaper_dir_->GetPath().Append(default_large_wallpaper_name),
            GetWallpaperFilePath());

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id);
  // Verify |SetDefaultWallpaper| removes the previously set custom wallpaper
  // info, and the small default wallpaper is set successfully with the correct
  // file path.
  UpdateDisplay("800x600");
  controller_->SetDefaultWallpaper(InitializeUser(account_id),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(wallpaper_dir_->GetPath().Append(default_small_wallpaper_name),
            GetWallpaperFilePath());

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);

  SimulateSettingCustomWallpaper(account_id);
  // Verify that when screen is rotated, |SetDefaultWallpaper| removes the
  // previously set custom wallpaper info, and the large default wallpaper is
  // set successfully with the correct file path.
  UpdateDisplay("1200x800/r");
  controller_->SetDefaultWallpaper(InitializeUser(account_id),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(wallpaper_dir_->GetPath().Append(default_large_wallpaper_name),
            GetWallpaperFilePath());

  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /*is_persistent=*/));
  // The user wallpaper info has been reset to the default value.
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForChildAccount) {
  CreateDefaultWallpapers();

  const std::string user_email = "child@test.com";
  const AccountId account_id = AccountId::FromUserEmail(user_email);
  SimulateUserLogin(user_email);

  // Verify the large child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("1600x1200");
  mojom::WallpaperUserInfoPtr wallpaper_user_info = InitializeUser(account_id);
  wallpaper_user_info->type = user_manager::USER_TYPE_CHILD;
  controller_->SetDefaultWallpaper(std::move(wallpaper_user_info),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(wallpaper_dir_->GetPath().Append(child_large_wallpaper_name),
            GetWallpaperFilePath());

  // Verify the small child wallpaper is set successfully with the correct file
  // path.
  UpdateDisplay("800x600");
  wallpaper_user_info = InitializeUser(account_id);
  wallpaper_user_info->type = user_manager::USER_TYPE_CHILD;
  controller_->SetDefaultWallpaper(std::move(wallpaper_user_info),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(wallpaper_dir_->GetPath().Append(child_small_wallpaper_name),
            GetWallpaperFilePath());
}

TEST_F(WallpaperControllerTest, SetDefaultWallpaperForGuestSession) {
  CreateDefaultWallpapers();

  // First, simulate setting a custom wallpaper for a regular user.
  const std::string user_email = "user@test.com";
  const AccountId regular_user_account_id =
      AccountId::FromUserEmail("user@test.com");
  SimulateUserLogin(user_email);
  SimulateSettingCustomWallpaper(regular_user_account_id);
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      regular_user_account_id, &wallpaper_info, true /*is_persistent=*/));
  wallpaper::WallpaperInfo default_wallpaper_info(
      std::string(), wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
      wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
  EXPECT_NE(wallpaper_info.type, default_wallpaper_info.type);

  // Simulate guest login.
  const std::string guest = "guest@test.com";
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(guest, user_manager::USER_TYPE_GUEST);
  session->SwitchActiveUser(AccountId::FromUserEmail(guest));
  session->SetSessionState(SessionState::ACTIVE);

  // Verify that during a guest session, |SetDefaultWallpaper| removes the user
  // custom wallpaper info, but a guest specific wallpaper should be set,
  // instead of the regular default wallpaper.
  UpdateDisplay("1600x1200");
  controller_->SetDefaultWallpaper(InitializeUser(regular_user_account_id),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(
      regular_user_account_id, &wallpaper_info, true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, default_wallpaper_info);
  EXPECT_EQ(wallpaper_dir_->GetPath().Append(guest_large_wallpaper_name),
            GetWallpaperFilePath());

  UpdateDisplay("800x600");
  controller_->SetDefaultWallpaper(InitializeUser(regular_user_account_id),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetWallpaperCount());
  EXPECT_EQ(wallpaper_dir_->GetPath().Append(guest_small_wallpaper_name),
            GetWallpaperFilePath());
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestInKioskMode) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  const std::string kiosk_app = "kiosk";
  const AccountId account_id = AccountId::FromUserEmail("user@test.com");

  // Simulate kiosk login.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession(kiosk_app, user_manager::USER_TYPE_KIOSK_APP);
  session->SwitchActiveUser(AccountId::FromUserEmail(kiosk_app));
  session->SetSessionState(SessionState::ACTIVE);

  // Verify that |SetCustomWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id), std::string() /* wallpaper_files_id */,
      "dummy_file_name", WALLPAPER_LAYOUT_CENTER, wallpaper::CUSTOMIZED,
      *image.bitmap(), true /* show_wallpaper */);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                 true /* is_persistent */));

  // Verify that |SetOnlineWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetOnlineWallpaper(InitializeUser(account_id), *image.bitmap(),
                                  "dummy_url", WALLPAPER_LAYOUT_CENTER,
                                  true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                 true /*is_persistent=*/));

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper in kiosk mode, and
  // |account_id|'s wallpaper info is not updated.
  controller_->SetDefaultWallpaper(InitializeUser(account_id),
                                   std::string() /*wallpaper_files_id=*/,
                                   true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_FALSE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                 true /*is_persistent=*/));
}

TEST_F(WallpaperControllerTest, IgnoreWallpaperRequestWhenPolicyIsEnforced) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  const std::string wallpaper_files_id = "dummy_file_id";
  const std::string user_email = "user@test.com";
  const AccountId account_id = AccountId::FromUserEmail("user@test.com");
  SimulateUserLogin(user_email);

  // Simulate setting a policy wallpaper by setting the wallpaper info.
  // TODO(crbug.com/776464): Replace this with a real |SetPolicyWallpaper| call.
  wallpaper::WallpaperInfo policy_wallpaper_info(
      "dummy_file_location", WALLPAPER_LAYOUT_CENTER, wallpaper::POLICY,
      base::Time::Now().LocalMidnight());
  controller_->SetUserWallpaperInfo(account_id, policy_wallpaper_info,
                                    true /*is_persistent=*/);
  EXPECT_TRUE(
      controller_->IsPolicyControlled(account_id, true /*is_persistent=*/));

  // Verify that |SetCustomWallpaper| doesn't set wallpaper when policy is
  // enforced, and |account_id|'s wallpaper info is not updated.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id), wallpaper_files_id, "dummy_file_name",
      WALLPAPER_LAYOUT_CENTER, wallpaper::CUSTOMIZED, *image.bitmap(),
      true /* show_wallpaper */);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  wallpaper::WallpaperInfo wallpaper_info;
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetOnlineWallpaper| doesn't set wallpaper when policy is
  // enforced, and |account_id|'s wallpaper info is not updated.
  controller_->SetOnlineWallpaper(InitializeUser(account_id), *image.bitmap(),
                                  "dummy_url", WALLPAPER_LAYOUT_CENTER,
                                  true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Verify that |SetDefaultWallpaper| doesn't set wallpaper when policy is
  // enforced, and |account_id|'s wallpaper info is not updated.
  controller_->SetDefaultWallpaper(
      InitializeUser(account_id), wallpaper_files_id, true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_EQ(0, GetWallpaperCount());
  EXPECT_TRUE(controller_->GetUserWallpaperInfo(account_id, &wallpaper_info,
                                                true /* is_persistent */));
  EXPECT_EQ(wallpaper_info, policy_wallpaper_info);

  // Remove the custom wallpaper directory that may be created.
  controller_->RemoveUserWallpaper(InitializeUser(account_id),
                                   wallpaper_files_id);
}

TEST_F(WallpaperControllerTest, VerifyWallpaperCache) {
  gfx::ImageSkia image = CreateImage(640, 480, kWallpaperColor);
  const std::string wallpaper_files_id = "dummy_file_id";
  const std::string user1 = "user1@test.com";
  const AccountId account_id1 = AccountId::FromUserEmail(user1);

  SimulateUserLogin(user1);

  // |user1| doesn't have wallpaper cache in the beginning.
  gfx::ImageSkia cached_wallpaper;
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id1, &cached_wallpaper));
  base::FilePath path;
  EXPECT_FALSE(controller_->GetPathFromCache(account_id1, &path));

  // Verify |SetOnlineWallpaper| updates wallpaper cache for |user1|.
  controller_->SetOnlineWallpaper(
      InitializeUser(account_id1), *image.bitmap(), "dummy_file_location",
      WALLPAPER_LAYOUT_CENTER, true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id1, &path));

  // After |user2| is logged in, |user1|'s wallpaper cache should still be kept
  // (crbug.com/339576). Note the active user is still |user1|.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->AddUserSession("user2@test.com");
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id1, &path));

  // Verify |SetDefaultWallpaper| clears wallpaper cache.
  controller_->SetDefaultWallpaper(InitializeUser(account_id1),
                                   wallpaper_files_id,
                                   true /*show_wallpaper=*/);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id1, &path));

  // Verify |SetCustomWallpaper| updates wallpaper cache for |user1|.
  controller_->SetCustomWallpaper(
      InitializeUser(account_id1), wallpaper_files_id, "dummy_file_name",
      WALLPAPER_LAYOUT_CENTER, wallpaper::CUSTOMIZED, *image.bitmap(),
      true /*show_wallpaper=*/);
  RunAllTasksUntilIdle();
  EXPECT_TRUE(
      controller_->GetWallpaperFromCache(account_id1, &cached_wallpaper));
  EXPECT_TRUE(controller_->GetPathFromCache(account_id1, &path));

  // Verify |RemoveUserWallpaper| clears wallpaper cache.
  controller_->RemoveUserWallpaper(InitializeUser(account_id1),
                                   wallpaper_files_id);
  EXPECT_FALSE(
      controller_->GetWallpaperFromCache(account_id1, &cached_wallpaper));
  EXPECT_FALSE(controller_->GetPathFromCache(account_id1, &path));
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

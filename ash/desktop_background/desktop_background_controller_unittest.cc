// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include <cmath>
#include <cstdlib>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_user_wallpaper_delegate.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/canvas.h"

using aura::RootWindow;
using aura::Window;
using wallpaper::WallpaperLayout;
using wallpaper::WALLPAPER_LAYOUT_CENTER;
using wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
using wallpaper::WALLPAPER_LAYOUT_STRETCH;
using wallpaper::WALLPAPER_LAYOUT_TILE;

namespace ash {
namespace {

// Containers IDs used for tests.
const int kDesktopBackgroundId = ash::kShellWindowId_DesktopBackgroundContainer;
const int kLockScreenBackgroundId =
    ash::kShellWindowId_LockScreenBackgroundContainer;

// Returns number of child windows in a shell window container.
int ChildCountForContainer(int container_id) {
  Window* root = ash::Shell::GetPrimaryRootWindow();
  Window* container = root->GetChildById(container_id);
  return static_cast<int>(container->children().size());
}

// Steps a widget's layer animation until it is completed. Animations must be
// enabled.
void RunAnimationForWidget(views::Widget* widget) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::Layer* layer = widget->GetNativeView()->layer();
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
  ~TaskObserver() override {}

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

void RunAllBlockingPoolTasksUntilIdle(base::SequencedWorkerPool* pool) {
  while (true) {
    pool->FlushForTesting();

    TaskObserver task_observer;
    base::MessageLoop::current()->AddTaskObserver(&task_observer);
    base::RunLoop().RunUntilIdle();
    base::MessageLoop::current()->RemoveTaskObserver(&task_observer);

    if (!task_observer.processed())
      break;
  }
}

}  // namespace

class DesktopBackgroundControllerTest : public test::AshTestBase {
 public:
  DesktopBackgroundControllerTest()
      : controller_(NULL),
        wallpaper_delegate_(NULL) {
  }
  ~DesktopBackgroundControllerTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    // Ash shell initialization creates wallpaper. Reset it so we can manually
    // control wallpaper creation and animation in our tests.
    RootWindowController* root_window_controller =
        Shell::GetPrimaryRootWindowController();
    root_window_controller->SetWallpaperController(NULL);
    root_window_controller->SetAnimatingWallpaperController(NULL);
    controller_ = Shell::GetInstance()->desktop_background_controller();
    wallpaper_delegate_ = static_cast<test::TestUserWallpaperDelegate*>(
        Shell::GetInstance()->user_wallpaper_delegate());
    controller_->set_wallpaper_reload_delay_for_test(0);
  }

  DesktopBackgroundView* desktop_background_view() {
    DesktopBackgroundWidgetController* controller =
        Shell::GetPrimaryRootWindowController()
            ->animating_wallpaper_controller()
            ->GetController(false);
    EXPECT_TRUE(controller);
    return static_cast<DesktopBackgroundView*>(
        controller->widget()->GetContentsView()->child_at(0));
  }

 protected:
  // A color that can be passed to CreateImage(). Specifically chosen to not
  // conflict with any of the default wallpaper colors.
  static const SkColor kCustomWallpaperColor = SK_ColorMAGENTA;

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
  void WallpaperFitToNativeResolution(DesktopBackgroundView* view,
                                      float device_scale_factor,
                                      int image_width,
                                      int image_height,
                                      SkColor color) {
    gfx::Size size = view->bounds().size();
    gfx::Canvas canvas(size, device_scale_factor, true);
    view->OnPaint(&canvas);

    int canvas_width = canvas.sk_canvas()->imageInfo().width();
    int canvas_height = canvas.sk_canvas()->imageInfo().height();
    SkBitmap bitmap;
    bitmap.allocN32Pixels(canvas_width, canvas_height);
    canvas.sk_canvas()->readPixels(&bitmap, 0, 0);

    for (int i = 0; i < canvas_width; i++) {
      for (int j = 0; j < canvas_height; j++) {
        if (i >= (canvas_width - image_width) / 2 &&
            i < (canvas_width + image_width) / 2 &&
            j >= (canvas_height - image_height) / 2 &&
            j < (canvas_height + image_height) / 2) {
          EXPECT_EQ(color, bitmap.getColor(i, j));
        } else {
          EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(i, j));
        }
      }
    }
  }

  // Runs kAnimatingDesktopController's animation to completion.
  // TODO(bshe): Don't require tests to run animations; it's slow.
  void RunDesktopControllerAnimation() {
    DesktopBackgroundWidgetController* controller =
        Shell::GetPrimaryRootWindowController()
            ->animating_wallpaper_controller()
            ->GetController(false);
    EXPECT_TRUE(controller);
    ASSERT_NO_FATAL_FAILURE(RunAnimationForWidget(controller->widget()));
  }

  DesktopBackgroundController* controller_;  // Not owned.

  test::TestUserWallpaperDelegate* wallpaper_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundControllerTest);
};

TEST_F(DesktopBackgroundControllerTest, BasicReparenting) {
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  controller->CreateEmptyWallpaper();

  // Wallpaper view/window exists in the desktop background container and
  // nothing is in the lock screen background container.
  EXPECT_EQ(1, ChildCountForContainer(kDesktopBackgroundId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenBackgroundId));

  // Moving background to lock container should succeed the first time but
  // subsequent calls should do nothing.
  EXPECT_TRUE(controller->MoveDesktopToLockedContainer());
  EXPECT_FALSE(controller->MoveDesktopToLockedContainer());

  // One window is moved from desktop to lock container.
  EXPECT_EQ(0, ChildCountForContainer(kDesktopBackgroundId));
  EXPECT_EQ(1, ChildCountForContainer(kLockScreenBackgroundId));

  // Moving background to desktop container should succeed the first time.
  EXPECT_TRUE(controller->MoveDesktopToUnlockedContainer());
  EXPECT_FALSE(controller->MoveDesktopToUnlockedContainer());

  // One window is moved from lock to desktop container.
  EXPECT_EQ(1, ChildCountForContainer(kDesktopBackgroundId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenBackgroundId));
}

TEST_F(DesktopBackgroundControllerTest, ControllerOwnership) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create wallpaper and background view.
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  controller->CreateEmptyWallpaper();

  // The new wallpaper is ready to start animating. kAnimatingDesktopController
  // holds the widget controller instance. kDesktopController will get it later.
  RootWindowController* root_window_controller =
      Shell::GetPrimaryRootWindowController();
  EXPECT_TRUE(root_window_controller->animating_wallpaper_controller()->
              GetController(false));

  // kDesktopController will receive the widget controller when the animation
  // is done.
  EXPECT_FALSE(root_window_controller->wallpaper_controller());

  // Force the widget's layer animation to play to completion.
  RunDesktopControllerAnimation();

  // Ownership has moved from kAnimatingDesktopController to kDesktopController.
  EXPECT_FALSE(root_window_controller->animating_wallpaper_controller()->
               GetController(false));
  EXPECT_TRUE(root_window_controller->wallpaper_controller());
}

// Test for crbug.com/149043 "Unlock screen, no launcher appears". Ensure we
// move all desktop views if there are more than one.
TEST_F(DesktopBackgroundControllerTest, BackgroundMovementDuringUnlock) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  controller->CreateEmptyWallpaper();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // User locks the screen, which moves the background forward.
  controller->MoveDesktopToLockedContainer();

  // Suspend/resume cycle causes wallpaper to refresh, loading a new desktop
  // background that will animate in on top of the old one.
  controller->CreateEmptyWallpaper();

  // In this state we have two desktop background views stored in different
  // properties. Both are in the lock screen background container.
  RootWindowController* root_window_controller =
      Shell::GetPrimaryRootWindowController();
  EXPECT_TRUE(root_window_controller->animating_wallpaper_controller()->
              GetController(false));
  EXPECT_TRUE(root_window_controller->wallpaper_controller());
  EXPECT_EQ(0, ChildCountForContainer(kDesktopBackgroundId));
  EXPECT_EQ(2, ChildCountForContainer(kLockScreenBackgroundId));

  // Before the wallpaper's animation completes, user unlocks the screen, which
  // moves the desktop to the back.
  controller->MoveDesktopToUnlockedContainer();

  // Ensure both desktop backgrounds have moved.
  EXPECT_EQ(2, ChildCountForContainer(kDesktopBackgroundId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenBackgroundId));

  // Finish the new desktop background animation.
  RunDesktopControllerAnimation();

  // Now there is one desktop background, in the back.
  EXPECT_EQ(1, ChildCountForContainer(kDesktopBackgroundId));
  EXPECT_EQ(0, ChildCountForContainer(kLockScreenBackgroundId));
}

// Test for crbug.com/156542. Animating wallpaper should immediately finish
// animation and replace current wallpaper before next animation starts.
TEST_F(DesktopBackgroundControllerTest, ChangeWallpaperQuick) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  controller->CreateEmptyWallpaper();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // Change to a new wallpaper.
  controller->CreateEmptyWallpaper();

  RootWindowController* root_window_controller =
      Shell::GetPrimaryRootWindowController();
  DesktopBackgroundWidgetController* animating_controller =
      root_window_controller->animating_wallpaper_controller()->GetController(
          false);
  EXPECT_TRUE(animating_controller);
  EXPECT_TRUE(root_window_controller->wallpaper_controller());

  // Change to another wallpaper before animation finished.
  controller->CreateEmptyWallpaper();

  // The animating controller should immediately move to desktop controller.
  EXPECT_EQ(animating_controller,
            root_window_controller->wallpaper_controller());

  // Cache the new animating controller.
  animating_controller = root_window_controller->
      animating_wallpaper_controller()->GetController(false);

  // Run wallpaper show animation to completion.
  ASSERT_NO_FATAL_FAILURE(
      RunAnimationForWidget(
          root_window_controller->animating_wallpaper_controller()->
              GetController(false)->widget()));

  EXPECT_TRUE(root_window_controller->wallpaper_controller());
  EXPECT_FALSE(root_window_controller->animating_wallpaper_controller()->
               GetController(false));
  // The desktop controller should be the last created animating controller.
  EXPECT_EQ(animating_controller,
            root_window_controller->wallpaper_controller());
}

TEST_F(DesktopBackgroundControllerTest, ResizeCustomWallpaper) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("320x200");

  gfx::ImageSkia image = CreateImage(640, 480, kCustomWallpaperColor);

  // Set the image as custom wallpaper, wait for the resize to finish, and check
  // that the resized image is the expected size.
  controller_->SetWallpaperImage(image, WALLPAPER_LAYOUT_STRETCH);
  EXPECT_TRUE(image.BackedBySameObjectAs(controller_->GetWallpaper()));
  RunAllBlockingPoolTasksUntilIdle(Shell::GetInstance()->blocking_pool());
  gfx::ImageSkia resized_image = controller_->GetWallpaper();
  EXPECT_FALSE(image.BackedBySameObjectAs(resized_image));
  EXPECT_EQ(gfx::Size(320, 200).ToString(), resized_image.size().ToString());

  // Load the original wallpaper again and check that we're still using the
  // previously-resized image instead of doing another resize
  // (http://crbug.com/321402).
  controller_->SetWallpaperImage(image, WALLPAPER_LAYOUT_STRETCH);
  RunAllBlockingPoolTasksUntilIdle(Shell::GetInstance()->blocking_pool());
  EXPECT_TRUE(resized_image.BackedBySameObjectAs(controller_->GetWallpaper()));
}

TEST_F(DesktopBackgroundControllerTest, GetMaxDisplaySize) {
  // Device scale factor shouldn't affect the native size.
  UpdateDisplay("1000x300*2");
  EXPECT_EQ(
      "1000x300",
      DesktopBackgroundController::GetMaxDisplaySizeInNative().ToString());

  // Rotated display should return the rotated size.
  UpdateDisplay("1000x300*2/r");
  EXPECT_EQ(
      "300x1000",
      DesktopBackgroundController::GetMaxDisplaySizeInNative().ToString());

  // UI Scaling shouldn't affect the native size.
  UpdateDisplay("1000x300*2@1.5");
  EXPECT_EQ(
      "1000x300",
      DesktopBackgroundController::GetMaxDisplaySizeInNative().ToString());

  if (!SupportsMultipleDisplays())
    return;

  // First display has maximum size.
  UpdateDisplay("400x300,100x100");
  EXPECT_EQ(
      "400x300",
      DesktopBackgroundController::GetMaxDisplaySizeInNative().ToString());

  // Second display has maximum size.
  UpdateDisplay("400x300,500x600");
  EXPECT_EQ(
      "500x600",
      DesktopBackgroundController::GetMaxDisplaySizeInNative().ToString());

  // Maximum width and height belongs to different displays.
  UpdateDisplay("400x300,100x500");
  EXPECT_EQ(
      "400x500",
      DesktopBackgroundController::GetMaxDisplaySizeInNative().ToString());
}

// Test that the wallpaper is always fitted to the native display resolution
// when the layout is WALLPAPER_LAYOUT_CENTER to prevent blurry images.
TEST_F(DesktopBackgroundControllerTest, DontSacleWallpaperWithCenterLayout) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Size high_resolution(3600, 2400);
  const gfx::Size low_resolution(360, 240);
  const float high_dsf = 2.0f;
  const float low_dsf = 1.0f;

  gfx::ImageSkia image_high_res = CreateImage(
      high_resolution.width(), high_resolution.height(), kCustomWallpaperColor);
  gfx::ImageSkia image_low_res = CreateImage(
      low_resolution.width(), low_resolution.height(), kCustomWallpaperColor);

  UpdateDisplay("1200x600*2");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 high resolution"));
    controller_->SetWallpaperImage(image_high_res, WALLPAPER_LAYOUT_CENTER);
    WallpaperFitToNativeResolution(
        desktop_background_view(), high_dsf, high_resolution.width(),
        high_resolution.height(), kCustomWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600*2 low resolution"));
    controller_->SetWallpaperImage(image_low_res, WALLPAPER_LAYOUT_CENTER);
    WallpaperFitToNativeResolution(
        desktop_background_view(), high_dsf, low_resolution.width(),
        low_resolution.height(), kCustomWallpaperColor);
  }

  UpdateDisplay("1200x600");
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 high resolution"));
    controller_->SetWallpaperImage(image_high_res, WALLPAPER_LAYOUT_CENTER);
    WallpaperFitToNativeResolution(
        desktop_background_view(), low_dsf, high_resolution.width(),
        high_resolution.height(), kCustomWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600 low resolution"));
    controller_->SetWallpaperImage(image_low_res, WALLPAPER_LAYOUT_CENTER);
    WallpaperFitToNativeResolution(
        desktop_background_view(), low_dsf, low_resolution.width(),
        low_resolution.height(), kCustomWallpaperColor);
  }

  UpdateDisplay("1200x600/u@1.5");  // 1.5 ui scale
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 high resolution"));
    controller_->SetWallpaperImage(image_high_res, WALLPAPER_LAYOUT_CENTER);
    WallpaperFitToNativeResolution(
        desktop_background_view(), low_dsf, high_resolution.width(),
        high_resolution.height(), kCustomWallpaperColor);
  }
  {
    SCOPED_TRACE(base::StringPrintf("1200x600/u@1.5 low resolution"));
    controller_->SetWallpaperImage(image_low_res, WALLPAPER_LAYOUT_CENTER);
    WallpaperFitToNativeResolution(
        desktop_background_view(), low_dsf, low_resolution.width(),
        low_resolution.height(), kCustomWallpaperColor);
  }
}

}  // namespace ash

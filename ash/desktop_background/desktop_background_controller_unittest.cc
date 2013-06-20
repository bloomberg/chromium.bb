// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include <cmath>
#include <cstdlib>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/codec/jpeg_codec.h"

using aura::RootWindow;
using aura::Window;

namespace ash {
namespace internal {

namespace {

// Containers IDs used for tests.
const int kDesktopBackgroundId =
    ash::internal::kShellWindowId_DesktopBackgroundContainer;
const int kLockScreenBackgroundId =
    ash::internal::kShellWindowId_LockScreenBackgroundContainer;

// Returns number of child windows in a shell window container.
int ChildCountForContainer(int container_id) {
  RootWindow* root = ash::Shell::GetPrimaryRootWindow();
  Window* container = root->GetChildById(container_id);
  return static_cast<int>(container->children().size());
}

class TestObserver : public DesktopBackgroundControllerObserver {
 public:
  explicit TestObserver(DesktopBackgroundController* controller)
      : controller_(controller) {
    DCHECK(controller_);
    controller_->AddObserver(this);
  }

  virtual ~TestObserver() {
    controller_->RemoveObserver(this);
  }

  void WaitForWallpaperDataChanged() {
    base::MessageLoop::current()->Run();
  }

  // DesktopBackgroundControllerObserver overrides:
  virtual void OnWallpaperDataChanged() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

 private:
  DesktopBackgroundController* controller_;
};

// Steps a widget's layer animation until it is completed. Animations must be
// enabled.
void RunAnimationForWidget(views::Widget* widget) {
  // Animations must be enabled for stepping to work.
  ASSERT_NE(ui::ScopedAnimationDurationScaleMode::duration_scale_mode(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::Layer* layer = widget->GetNativeView()->layer();
  ui::LayerAnimatorTestController controller(layer->GetAnimator());
  ui::AnimationContainerElement* element = layer->GetAnimator();
  // Multiple steps are required to complete complex animations.
  // TODO(vollick): This should not be necessary. crbug.com/154017
  while (controller.animator()->is_animating()) {
    controller.StartThreadedAnimationsIfNeeded();
    base::TimeTicks step_time = controller.animator()->last_step_time();
    element->Step(step_time + base::TimeDelta::FromMilliseconds(1000));
  }
}

}  // namespace

class DesktopBackgroundControllerTest : public test::AshTestBase {
 public:
  DesktopBackgroundControllerTest()
      : ui_thread_(content::BrowserThread::UI, base::MessageLoop::current()) {
  }
  virtual ~DesktopBackgroundControllerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    // Ash shell initialization creates wallpaper. Reset it so we can manually
    // control wallpaper creation and animation in our tests.
    RootWindow* root = Shell::GetPrimaryRootWindow();
    root->SetProperty(kDesktopController,
        static_cast<DesktopBackgroundWidgetController*>(NULL));
    root->SetProperty(kAnimatingDesktopController,
        static_cast<AnimatingDesktopController*>(NULL));
  }

 protected:
  // Runs kAnimatingDesktopController's animation to completion.
  // TODO(bshe): Don't require tests to run animations; it's slow.
  void RunDesktopControllerAnimation() {
    RootWindow* root = Shell::GetPrimaryRootWindow();
    DesktopBackgroundWidgetController* controller =
        root->GetProperty(kAnimatingDesktopController)->GetController(false);
    ASSERT_NO_FATAL_FAILURE(RunAnimationForWidget(controller->widget()));
  }

  // Returns true if |image|'s color at (0, 0) is close to |expected_color|.
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
    SkColor image_color = bitmap->getColor(0, 0);
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
            gfx::JPEGCodec::FORMAT_SkBitmap, width, height, bitmap.rowBytes(),
            kQuality, &output)) {
      LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
      return false;
    }

    size_t bytes_written = file_util::WriteFile(
        path, reinterpret_cast<const char*>(&output[0]), output.size());
    if (bytes_written != output.size()) {
      LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
                 << output.size() << " to " << path.value();
      return false;
    }

    return true;
  }

 private:
  content::TestBrowserThread ui_thread_;

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
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Create wallpaper and background view.
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  controller->CreateEmptyWallpaper();

  // The new wallpaper is ready to start animating. kAnimatingDesktopController
  // holds the widget controller instance. kDesktopController will get it later.
  RootWindow* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(
      root->GetProperty(kAnimatingDesktopController)->GetController(false));

  // kDesktopController will receive the widget controller when the animation
  // is done.
  EXPECT_FALSE(root->GetProperty(kDesktopController));

  // Force the widget's layer animation to play to completion.
  RunDesktopControllerAnimation();

  // Ownership has moved from kAnimatingDesktopController to kDesktopController.
  EXPECT_FALSE(
      root->GetProperty(kAnimatingDesktopController)->GetController(false));
  EXPECT_TRUE(root->GetProperty(kDesktopController));
}

// Test for crbug.com/149043 "Unlock screen, no launcher appears". Ensure we
// move all desktop views if there are more than one.
TEST_F(DesktopBackgroundControllerTest, BackgroundMovementDuringUnlock) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

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
  RootWindow* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(
      root->GetProperty(kAnimatingDesktopController)->GetController(false));
  EXPECT_TRUE(root->GetProperty(kDesktopController));
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
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Reset wallpaper state, see ControllerOwnership above.
  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  controller->CreateEmptyWallpaper();

  // Run wallpaper show animation to completion.
  RunDesktopControllerAnimation();

  // Change to a new wallpaper.
  controller->CreateEmptyWallpaper();

  RootWindow* root = Shell::GetPrimaryRootWindow();
  DesktopBackgroundWidgetController* animating_controller =
      root->GetProperty(kAnimatingDesktopController)->GetController(false);
  EXPECT_TRUE(animating_controller);
  EXPECT_TRUE(root->GetProperty(kDesktopController));

  // Change to another wallpaper before animation finished.
  controller->CreateEmptyWallpaper();

  // The animating controller should immediately move to desktop controller.
  EXPECT_EQ(animating_controller, root->GetProperty(kDesktopController));

  // Cache the new animating controller.
  animating_controller =
      root->GetProperty(kAnimatingDesktopController)->GetController(false);

  // Run wallpaper show animation to completion.
  ASSERT_NO_FATAL_FAILURE(
      RunAnimationForWidget(
          root->GetProperty(kAnimatingDesktopController)->GetController(false)->
              widget()));

  EXPECT_TRUE(root->GetProperty(kDesktopController));
  EXPECT_FALSE(
      root->GetProperty(kAnimatingDesktopController)->GetController(false));
  // The desktop controller should be the last created animating controller.
  EXPECT_EQ(animating_controller, root->GetProperty(kDesktopController));
}

// Tests that SetDefaultWallpaper() loads default wallpaper from disk when
// instructed to do so via command-line flags.
TEST_F(DesktopBackgroundControllerTest, SetDefaultWallpaper) {
  // We cannot short-circuit animations for this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  TestObserver observer(controller);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath kLargePath =
      temp_dir.path().Append(FILE_PATH_LITERAL("large.jpg"));
  const SkColor kLargeColor = SK_ColorRED;
  ASSERT_TRUE(WriteJPEGFile(kLargePath, kLargeWallpaperMaxWidth,
      kLargeWallpaperMaxHeight, kLargeColor));

  const base::FilePath kSmallPath =
      temp_dir.path().Append(FILE_PATH_LITERAL("small.jpg"));
  const SkColor kSmallColor = SK_ColorGREEN;
  ASSERT_TRUE(WriteJPEGFile(kSmallPath, kSmallWallpaperMaxWidth,
      kSmallWallpaperMaxHeight, kSmallColor));

  const base::FilePath kLargeGuestPath =
      temp_dir.path().Append(FILE_PATH_LITERAL("guest_large.jpg"));
  const SkColor kLargeGuestColor = SK_ColorBLUE;
  ASSERT_TRUE(WriteJPEGFile(kLargeGuestPath, kLargeWallpaperMaxWidth,
      kLargeWallpaperMaxHeight, kLargeGuestColor));

  const base::FilePath kSmallGuestPath =
      temp_dir.path().Append(FILE_PATH_LITERAL("guest_small.jpg"));
  const SkColor kSmallGuestColor = SK_ColorYELLOW;
  ASSERT_TRUE(WriteJPEGFile(kSmallGuestPath, kSmallWallpaperMaxWidth,
      kSmallWallpaperMaxHeight, kSmallGuestColor));

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchPath(
      switches::kAshDefaultWallpaperLarge, kLargePath);
  command_line.AppendSwitchPath(
      switches::kAshDefaultWallpaperSmall, kSmallPath);
  command_line.AppendSwitchPath(
      switches::kAshDefaultGuestWallpaperLarge, kLargeGuestPath);
  command_line.AppendSwitchPath(
      switches::kAshDefaultGuestWallpaperSmall, kSmallGuestPath);
  controller->set_command_line_for_testing(&command_line);

  // At 800x600, the small wallpaper should be loaded.
  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay("800x600");
  ASSERT_TRUE(controller->SetDefaultWallpaper(false));
  observer.WaitForWallpaperDataChanged();
  RunDesktopControllerAnimation();
  EXPECT_TRUE(ImageIsNearColor(controller->GetWallpaper(), kSmallColor));

  // Requesting the same wallpaper again should be a no-op.
  EXPECT_FALSE(controller->SetDefaultWallpaper(false));

  // It should still be a no-op after adding a second small display (i.e.
  // the controller should continue using the same wallpaper).
  display_manager_test_api.UpdateDisplay("800x600,800x600");
  EXPECT_FALSE(controller->SetDefaultWallpaper(false));

  // Switch to guest mode and check that the small guest wallpaper is loaded.
  ASSERT_TRUE(controller->SetDefaultWallpaper(true));
  observer.WaitForWallpaperDataChanged();
  EXPECT_TRUE(ImageIsNearColor(controller->GetWallpaper(), kSmallGuestColor));

  // Switch to a larger display and check that the large wallpaper is loaded.
  display_manager_test_api.UpdateDisplay("1600x1200");
  ASSERT_TRUE(controller->SetDefaultWallpaper(false));
  observer.WaitForWallpaperDataChanged();
  RunDesktopControllerAnimation();
  EXPECT_TRUE(ImageIsNearColor(controller->GetWallpaper(), kLargeColor));

  // Switch to the guest wallpaper while still at a higher resolution.
  ASSERT_TRUE(controller->SetDefaultWallpaper(true));
  observer.WaitForWallpaperDataChanged();
  RunDesktopControllerAnimation();
  EXPECT_TRUE(ImageIsNearColor(controller->GetWallpaper(), kLargeGuestColor));
}

}  // namespace internal
}  // namespace ash

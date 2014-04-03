// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include <cmath>
#include <cstdlib>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_user_wallpaper_delegate.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

using aura::RootWindow;
using aura::Window;

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
  gfx::AnimationContainerElement* element = layer->GetAnimator();
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
      : command_line_(CommandLine::NO_PROGRAM),
        controller_(NULL) {
  }
  virtual ~DesktopBackgroundControllerTest() {}

  virtual void SetUp() OVERRIDE {
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

 protected:
  // Colors used for different default wallpapers by
  // WriteWallpapersAndSetFlags().
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

  // Creates an image of size |size|.
  gfx::ImageSkia CreateImage(int width, int height, SkColor color) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseColor(color);
    gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    return image;
  }

  // Runs kAnimatingDesktopController's animation to completion.
  // TODO(bshe): Don't require tests to run animations; it's slow.
  void RunDesktopControllerAnimation() {
    DesktopBackgroundWidgetController* controller =
        Shell::GetPrimaryRootWindowController()
            ->animating_wallpaper_controller()
            ->GetController(false);
    ASSERT_NO_FATAL_FAILURE(RunAnimationForWidget(controller->widget()));
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

    size_t bytes_written = base::WriteFile(
        path, reinterpret_cast<const char*>(&output[0]), output.size());
    if (bytes_written != output.size()) {
      LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
                 << output.size() << " to " << path.value();
      return false;
    }

    return true;
  }

  // Initializes |wallpaper_dir_|, writes JPEG wallpaper images to it, and
  // passes |controller_| a command line instructing it to use the images.
  // Only needs to be called (once) by tests that want to test loading of
  // default wallpapers.
  void WriteWallpapersAndSetFlags() {
    wallpaper_dir_.reset(new base::ScopedTempDir);
    ASSERT_TRUE(wallpaper_dir_->CreateUniqueTempDir());

    const base::FilePath kLargePath =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("large.jpg"));
    ASSERT_TRUE(WriteJPEGFile(kLargePath, kWallpaperSize, kWallpaperSize,
                              kLargeWallpaperColor));
    command_line_.AppendSwitchPath(
        switches::kAshDefaultWallpaperLarge, kLargePath);

    const base::FilePath kSmallPath =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("small.jpg"));
    ASSERT_TRUE(WriteJPEGFile(kSmallPath, kWallpaperSize, kWallpaperSize,
                              kSmallWallpaperColor));
    command_line_.AppendSwitchPath(
        switches::kAshDefaultWallpaperSmall, kSmallPath);

    const base::FilePath kLargeGuestPath =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("guest_large.jpg"));
    ASSERT_TRUE(WriteJPEGFile(kLargeGuestPath, kWallpaperSize, kWallpaperSize,
                              kLargeGuestWallpaperColor));
    command_line_.AppendSwitchPath(
        switches::kAshGuestWallpaperLarge, kLargeGuestPath);

    const base::FilePath kSmallGuestPath =
        wallpaper_dir_->path().Append(FILE_PATH_LITERAL("guest_small.jpg"));
    ASSERT_TRUE(WriteJPEGFile(kSmallGuestPath, kWallpaperSize, kWallpaperSize,
                              kSmallGuestWallpaperColor));
    command_line_.AppendSwitchPath(
        switches::kAshGuestWallpaperSmall, kSmallGuestPath);

    controller_->set_command_line_for_testing(&command_line_);
  }

  // Custom command line passed to DesktopBackgroundController by
  // WriteWallpapersAndSetFlags().
  CommandLine command_line_;

  // Directory created by WriteWallpapersAndSetFlags() to store default
  // wallpaper images.
  scoped_ptr<base::ScopedTempDir> wallpaper_dir_;

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
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

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

TEST_F(DesktopBackgroundControllerTest, DisplayChange) {
  // TODO(derat|oshima|bshe): Host windows can't be resized on Win8.
  if (!SupportsHostWindowResize())
    return;

  // Set the wallpaper to ensure that UpdateWallpaper() will be called when the
  // display configuration changes.
  gfx::ImageSkia image = CreateImage(640, 480, kCustomWallpaperColor);
  wallpaper_delegate_->set_custom_wallpaper(image);
  controller_->SetCustomWallpaper(image, WALLPAPER_LAYOUT_STRETCH);

  // Small wallpaper images should be used for configurations less than or
  // equal to kSmallWallpaperMaxWidth by kSmallWallpaperMaxHeight, even if
  // multiple displays are connected.
  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay("800x600");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_SMALL,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(0, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  display_manager_test_api.UpdateDisplay("800x600,800x600");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_SMALL,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(0, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  display_manager_test_api.UpdateDisplay("1366x800");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_SMALL,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(1, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  // At larger sizes, large wallpapers should be used.
  display_manager_test_api.UpdateDisplay("1367x800");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_LARGE,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(1, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  display_manager_test_api.UpdateDisplay("1367x801");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_LARGE,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(1, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  display_manager_test_api.UpdateDisplay("2560x1700");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_LARGE,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(1, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  // Rotated smaller screen may use larger image.
  display_manager_test_api.UpdateDisplay("800x600/r");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_SMALL,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(1, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  display_manager_test_api.UpdateDisplay("800x600/r,800x600");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_SMALL,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(1, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());
  display_manager_test_api.UpdateDisplay("1366x800/r");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(WALLPAPER_RESOLUTION_LARGE,
            controller_->GetAppropriateResolution());
  EXPECT_EQ(1, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());

  // Max display size didn't chagne.
  display_manager_test_api.UpdateDisplay("900x800/r,400x1366");
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0, wallpaper_delegate_->GetUpdateWallpaperCountAndReset());
}

// Test that DesktopBackgroundController loads the appropriate wallpaper
// images as specified via command-line flags in various situations.
// Splitting these into separate tests avoids needing to run animations.
// TODO(derat): Combine these into a single test -- see
// RunDesktopControllerAnimation()'s TODO.
TEST_F(DesktopBackgroundControllerTest, SmallDefaultWallpaper) {
  if (!SupportsMultipleDisplays())
    return;

  WriteWallpapersAndSetFlags();
  TestObserver observer(controller_);

  // At 800x600, the small wallpaper should be loaded.
  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay("800x600");
  ASSERT_TRUE(controller_->SetDefaultWallpaper(false));
  observer.WaitForWallpaperDataChanged();
  EXPECT_TRUE(ImageIsNearColor(controller_->GetWallpaper(),
                               kSmallWallpaperColor));

  // Requesting the same wallpaper again should be a no-op.
  ASSERT_FALSE(controller_->SetDefaultWallpaper(false));
}

TEST_F(DesktopBackgroundControllerTest, LargeDefaultWallpaper) {
  if (!SupportsMultipleDisplays())
    return;

  WriteWallpapersAndSetFlags();
  TestObserver observer(controller_);
  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay("1600x1200");
  ASSERT_TRUE(controller_->SetDefaultWallpaper(false));
  observer.WaitForWallpaperDataChanged();
  EXPECT_TRUE(ImageIsNearColor(controller_->GetWallpaper(),
                               kLargeWallpaperColor));
}

TEST_F(DesktopBackgroundControllerTest, LargeDefaultWallpaperWhenRotated) {
  if (!SupportsMultipleDisplays())
    return;
  WriteWallpapersAndSetFlags();
  TestObserver observer(controller_);
  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());

  display_manager_test_api.UpdateDisplay("1200x800/r");
  ASSERT_TRUE(controller_->SetDefaultWallpaper(false));
  observer.WaitForWallpaperDataChanged();
  EXPECT_TRUE(ImageIsNearColor(controller_->GetWallpaper(),
                               kLargeWallpaperColor));
}

TEST_F(DesktopBackgroundControllerTest, SmallGuestWallpaper) {
  if (!SupportsMultipleDisplays())
    return;

  WriteWallpapersAndSetFlags();
  TestObserver observer(controller_);
  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay("800x600");
  ASSERT_TRUE(controller_->SetDefaultWallpaper(true));
  observer.WaitForWallpaperDataChanged();
  EXPECT_TRUE(ImageIsNearColor(controller_->GetWallpaper(),
                               kSmallGuestWallpaperColor));
}

TEST_F(DesktopBackgroundControllerTest, LargeGuestWallpaper) {
  if (!SupportsMultipleDisplays())
    return;

  WriteWallpapersAndSetFlags();
  TestObserver observer(controller_);
  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay("1600x1200");
  ASSERT_TRUE(controller_->SetDefaultWallpaper(true));
  observer.WaitForWallpaperDataChanged();
  EXPECT_TRUE(ImageIsNearColor(controller_->GetWallpaper(),
                               kLargeGuestWallpaperColor));
}

TEST_F(DesktopBackgroundControllerTest, ResizeCustomWallpaper) {
  if (!SupportsMultipleDisplays())
    return;

  test::DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay("320x200");

  gfx::ImageSkia image = CreateImage(640, 480, kCustomWallpaperColor);

  // Set the image as custom wallpaper, wait for the resize to finish, and check
  // that the resized image is the expected size.
  controller_->SetCustomWallpaper(image, WALLPAPER_LAYOUT_STRETCH);
  EXPECT_TRUE(image.BackedBySameObjectAs(controller_->GetWallpaper()));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  content::RunAllPendingInMessageLoop();
  gfx::ImageSkia resized_image = controller_->GetWallpaper();
  EXPECT_FALSE(image.BackedBySameObjectAs(resized_image));
  EXPECT_EQ(gfx::Size(320, 200).ToString(), resized_image.size().ToString());

  // Load the original wallpaper again and check that we're still using the
  // previously-resized image instead of doing another resize
  // (http://crbug.com/321402).
  controller_->SetCustomWallpaper(image, WALLPAPER_LAYOUT_STRETCH);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  content::RunAllPendingInMessageLoop();
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

TEST_F(DesktopBackgroundControllerTest, SwitchBetweenDefaultAndCustom) {
  // Start loading the default wallpaper.
  UpdateDisplay("640x480");
  WriteWallpapersAndSetFlags();
  ASSERT_TRUE(controller_->SetDefaultWallpaper(false));

  // Custom wallpaper should be applied immediately, canceling the default
  // wallpaper load task.
  gfx::ImageSkia image = CreateImage(640, 480, kCustomWallpaperColor);
  controller_->SetCustomWallpaper(image, WALLPAPER_LAYOUT_STRETCH);
  EXPECT_TRUE(ImageIsNearColor(controller_->GetWallpaper(),
                               kCustomWallpaperColor));

  // A call to SetDefaultWallpaper() should return true now, indicating that a
  // new load task was started (since the previous one was interrupted by
  // SetCustomWallpaper()). See http://crbug.com/327443.
  TestObserver observer(controller_);
  ASSERT_TRUE(controller_->SetDefaultWallpaper(false));
  observer.WaitForWallpaperDataChanged();
  EXPECT_TRUE(ImageIsNearColor(controller_->GetWallpaper(),
                               kSmallWallpaperColor));
}

}  // namespace ash

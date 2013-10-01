// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"

using ui::ScopedAnimationDurationScaleMode;

namespace {

// Returns the bounds of |view| in widget coordinates.
gfx::Rect GetRectInWidget(views::View* view) {
  return view->ConvertRectToWidget(view->GetLocalBounds());
}

}  // namespace

// TODO(jamescook): If immersive mode becomes popular on CrOS, consider porting
// it to other Aura platforms (win_aura, linux_aura).  http://crbug.com/163931
#if defined(OS_CHROMEOS)

class ImmersiveModeControllerAshTest : public InProcessBrowserTest {
 public:
  ImmersiveModeControllerAshTest() : browser_view_(NULL), controller_(NULL) {}
  virtual ~ImmersiveModeControllerAshTest() {}

  BrowserView* browser_view() { return browser_view_; }
  ImmersiveModeControllerAsh* controller() { return controller_; }

  // content::BrowserTestBase overrides:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ImmersiveFullscreenConfiguration::EnableImmersiveFullscreenForTest();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(ImmersiveFullscreenConfiguration::UseImmersiveFullscreen());
    browser_view_ = static_cast<BrowserView*>(browser()->window());
    controller_ = static_cast<ImmersiveModeControllerAsh*>(
        browser_view_->immersive_mode_controller());
    controller_->DisableAnimationsForTest();
    zero_duration_mode_.reset(new ScopedAnimationDurationScaleMode(
        ScopedAnimationDurationScaleMode::ZERO_DURATION));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    zero_duration_mode_.reset();
  }

 private:
  BrowserView* browser_view_;
  ImmersiveModeControllerAsh* controller_;
  scoped_ptr<ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTest);
};

// Test behavior when the mouse becomes hovered without moving.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest,
                       MouseHoveredWithoutMoving) {
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(controller()->IsEnabled());

  scoped_ptr<ImmersiveRevealedLock> lock;

  // 1) Test that if the mouse becomes hovered without the mouse moving due to a
  // lock causing the top-of-window views to be revealed (and the mouse
  // happening to be near the top of the screen), the top-of-window views do not
  // hide till the mouse moves off of the top-of-window views.
  controller()->SetMouseHoveredForTest(true);
  EXPECT_FALSE(controller()->IsRevealed());
  lock.reset(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_TRUE(controller()->IsRevealed());
  controller()->SetMouseHoveredForTest(false);
  EXPECT_FALSE(controller()->IsRevealed());

  // 2) Test that if the mouse becomes hovered without moving because of a
  // reveal in ImmersiveModeController::SetEnabled(true) and there are no locks
  // keeping the top-of-window views revealed, that mouse hover does not prevent
  // the top-of-window views from closing.
  chrome::ToggleFullscreenMode(browser());
  controller()->SetMouseHoveredForTest(true);
  EXPECT_FALSE(controller()->IsRevealed());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(controller()->IsRevealed());

  // 3) Test that if the mouse becomes hovered without moving because of a
  // reveal in ImmersiveModeController::SetEnabled(true) and there is a lock
  // keeping the top-of-window views revealed, that the top-of-window views do
  // not hide till the mouse moves off of the top-of-window views.
  chrome::ToggleFullscreenMode(browser());
  controller()->SetMouseHoveredForTest(true);
  lock.reset(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_FALSE(controller()->IsRevealed());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(controller()->IsRevealed());
  lock.reset();
  EXPECT_TRUE(controller()->IsRevealed());
  controller()->SetMouseHoveredForTest(false);
  EXPECT_FALSE(controller()->IsRevealed());
}

// GetRevealedLock() specific tests.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest, RevealedLock) {
  scoped_ptr<ImmersiveRevealedLock> lock1;
  scoped_ptr<ImmersiveRevealedLock> lock2;

  // Immersive mode is not on by default.
  EXPECT_FALSE(controller()->IsEnabled());

  // Move the mouse out of the way.
  controller()->SetMouseHoveredForTest(false);

  // 1) Test acquiring and releasing a revealed state lock while immersive mode
  // is disabled. Acquiring or releasing the lock should have no effect till
  // immersive mode is enabled.
  lock1.reset(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());

  // Immersive mode should start in the revealed state due to the lock.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());

  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());

  lock1.reset();
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_FALSE(controller()->ShouldHideTopViews());

  // Immersive mode should start in the closed state because the lock is no
  // longer held.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(controller()->ShouldHideTopViews());

  // 2) Test that acquiring a revealed state lock reveals the top-of-window
  // views if they are hidden.
  EXPECT_FALSE(controller()->IsRevealed());
  lock1.reset(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());

  // 3) Test that the top-of-window views are only hidden when all of the locks
  // are released.
  lock2.reset(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  lock1.reset();
  EXPECT_TRUE(controller()->IsRevealed());

  lock2.reset();
  EXPECT_FALSE(controller()->IsRevealed());
}

// Shelf-specific immersive mode tests.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest, ImmersiveShelf) {
  // Shelf is visible when the test starts.
  ash::internal::ShelfLayoutManager* shelf =
      ash::Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  ASSERT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // Turning immersive mode on sets the shelf to auto-hide.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view()->IsFullscreen());
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive mode puts it back.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // The user could toggle the launcher behavior.
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Enabling immersive mode keeps auto-hide.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view()->IsFullscreen());
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());

  // Disabling immersive mode maintains the user's auto-hide selection.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());
}

// Test how being simultaneously in tab fullscreen and immersive fullscreen
// affects the shelf visibility and whether the tab indicators are hidden.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest,
                       TabAndBrowserFullscreen) {
  ash::internal::ShelfLayoutManager* shelf =
      ash::Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();

  controller()->SetForceHideTabIndicatorsForTest(false);

  // The shelf should start out as visible.
  ASSERT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());

  // 1) Test that entering tab fullscreen from immersive mode hides the tab
  // indicators and the shelf.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_FALSE(controller()->ShouldHideTabIndicators());

  // The shelf visibility and the tab indicator visibility are updated as a
  // result of NOTIFICATION_FULLSCREEN_CHANGED which is asynchronous. Wait for
  // the notification before testing visibility.
  scoped_ptr<FullscreenNotificationObserver> waiter(
      new FullscreenNotificationObserver());

  browser()->fullscreen_controller()->ToggleFullscreenModeForTab(
      browser_view()->GetActiveWebContents(), true);
  waiter->Wait();
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_TRUE(controller()->ShouldHideTabIndicators());

  // 2) Test that exiting tab fullscreen shows the tab indicators and autohides
  // the shelf.
  waiter.reset(new FullscreenNotificationObserver());
  browser()->fullscreen_controller()->ToggleFullscreenModeForTab(
      browser_view()->GetActiveWebContents(), false);
  waiter->Wait();
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_FALSE(controller()->ShouldHideTabIndicators());

  // 3) Test that exiting tab fullscreen and immersive fullscreen
  // simultaneously correctly updates the shelf visibility and whether the tab
  // indicators should be hidden.
  waiter.reset(new FullscreenNotificationObserver());
  browser()->fullscreen_controller()->ToggleFullscreenModeForTab(
      browser_view()->GetActiveWebContents(), true);
  waiter->Wait();
  waiter.reset(new FullscreenNotificationObserver());
  chrome::ToggleFullscreenMode(browser());
  waiter->Wait();

  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_EQ(ash::SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_TRUE(controller()->ShouldHideTabIndicators());
}

// Validate top container touch insets are being updated at the correct time in
// immersive mode.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest,
                       ImmersiveTopContainerInsets) {
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  aura::Window* window = contents->GetView()->GetContentNativeView();

  // Turning immersive mode on sets positive top touch insets on the render view
  // window.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view()->IsFullscreen());
  ASSERT_TRUE(controller()->IsEnabled());
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() > 0);

  // Trigger a reveal resets insets as now the touch target for the top
  // container is large enough.
  controller()->StartRevealForTest(true);
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() == 0);

  // End reveal by moving the mouse off the top-of-window views. We
  // should see the top insets being positive again to allow a bigger touch
  // target.
  controller()->SetMouseHoveredForTest(false);
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() > 0);

  // Disabling immersive mode resets the top touch insets to 0.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() == 0);
}

#endif  // defined(OS_CHROMEOS)

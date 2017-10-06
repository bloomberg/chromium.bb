// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

class ImmersiveModeControllerAshHostedAppBrowserTest
    : public InProcessBrowserTest {
 public:
  ImmersiveModeControllerAshHostedAppBrowserTest() = default;
  ~ImmersiveModeControllerAshHostedAppBrowserTest() override = default;

  // InProcessBrowserTest override:
  void SetUpOnMainThread() override {
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        "test_browser_app", true /* trusted_source */, gfx::Rect(),
        InProcessBrowserTest::browser()->profile(), true);
    browser_ = new Browser(params);
    controller_ = BrowserView::GetBrowserViewForBrowser(browser_)
                      ->immersive_mode_controller();
    browser_->window()->Show();
  }

  // Toggle the browser's fullscreen state.
  void ToggleFullscreen() {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously. The notification
    // is used to trigger changes in whether the shelf is auto hidden.
    std::unique_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }

  Browser* browser() { return browser_; }
  ImmersiveModeController* controller() { return controller_; }

 private:
  // Not owned.
  Browser* browser_;
  ImmersiveModeController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshHostedAppBrowserTest);
};

// Verify the immersive mode status is as expected in tablet mode (titlebars are
// autohidden in tablet mode).
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshHostedAppBrowserTest,
                       ImmersiveModeStatusTabletMode) {
  ASSERT_FALSE(controller()->IsEnabled());

  // Verify that after entering tablet mode, immersive mode is enabled.
  ash::TabletModeController* tablet_mode_controller =
      ash::Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->EnableTabletModeWindowManager(true);
  tablet_mode_controller->FlushForTesting();
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that after minimizing, immersive mode is disabled.
  browser()->window()->Minimize();
  EXPECT_FALSE(controller()->IsEnabled());

  // Verify that after showing the browser, immersive mode is reenabled.
  browser()->window()->Show();
  tablet_mode_controller->FlushForTesting();
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that immersive mode remains if fullscreen is toggled while in tablet
  // mode.
  ToggleFullscreen();
  EXPECT_TRUE(controller()->IsEnabled());
  tablet_mode_controller->EnableTabletModeWindowManager(false);
  tablet_mode_controller->FlushForTesting();
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that immersive mode remains if the browser was fullscreened when
  // entering tablet mode.
  tablet_mode_controller->EnableTabletModeWindowManager(true);
  tablet_mode_controller->FlushForTesting();
  EXPECT_TRUE(controller()->IsEnabled());

  // Verify that if the browser is not fullscreened, upon exiting tablet mode,
  // immersive mode is not enabled.
  ToggleFullscreen();
  EXPECT_TRUE(controller()->IsEnabled());
  tablet_mode_controller->EnableTabletModeWindowManager(false);
  tablet_mode_controller->FlushForTesting();
  EXPECT_FALSE(controller()->IsEnabled());
}

// Verify that the frame layout is as expected when using immersive mode in
// tablet mode.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshHostedAppBrowserTest,
                       FrameLayout) {
  ASSERT_FALSE(controller()->IsEnabled());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          browser_view->GetWidget()->non_client_view()->frame_view());
  ash::FrameCaptionButtonContainerView* caption_button_container =
      frame_view->caption_button_container_;
  ash::FrameCaptionButtonContainerView::TestApi frame_test_api(
      caption_button_container);

  EXPECT_TRUE(frame_test_api.size_button()->visible());

  // Verify the size button is hidden in tablet mode.
  ash::TabletModeController* tablet_mode_controller =
      ash::Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->EnableTabletModeWindowManager(true);
  tablet_mode_controller->FlushForTesting();
  frame_test_api.EndAnimations();

  EXPECT_FALSE(frame_test_api.size_button()->visible());

  // Verify the size button is visible in clamshell mode, and that it does not
  // cover the other two buttons.
  tablet_mode_controller->EnableTabletModeWindowManager(false);
  tablet_mode_controller->FlushForTesting();
  frame_test_api.EndAnimations();

  EXPECT_TRUE(frame_test_api.size_button()->visible());
  EXPECT_FALSE(frame_test_api.size_button()->GetBoundsInScreen().Intersects(
      frame_test_api.close_button()->GetBoundsInScreen()));
  EXPECT_FALSE(frame_test_api.size_button()->GetBoundsInScreen().Intersects(
      frame_test_api.minimize_button()->GetBoundsInScreen()));
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
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
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

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
    controller_ = browser_view()->immersive_mode_controller();

    // Disable animations in immersive fullscreen before we show the window,
    // which triggers an animation.
    ash::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerAsh*>(controller_)->controller())
        .SetupForTest();

    browser_->window()->Show();
  }

  // Returns the bounds of |view| in widget coordinates.
  gfx::Rect GetBoundsInWidget(views::View* view) {
    return view->ConvertRectToWidget(view->GetLocalBounds());
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

  // Attempt revealing the top-of-window views.
  void AttemptReveal() {
    if (!revealed_lock_.get()) {
      revealed_lock_.reset(controller_->GetRevealedLock(
          ImmersiveModeControllerAsh::ANIMATE_REVEAL_NO));
    }
  }

  Browser* browser() { return browser_; }
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser_);
  }
  ImmersiveModeController* controller() { return controller_; }

 private:
  // Not owned.
  Browser* browser_;
  ImmersiveModeController* controller_;

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshHostedAppBrowserTest);
};

// Test the layout and visibility of the TopContainerView and web contents when
// a hosted app is put into immersive fullscreen.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshHostedAppBrowserTest, Layout) {
  TabStrip* tabstrip = browser_view()->tabstrip();
  ToolbarView* toolbar = browser_view()->toolbar();
  views::WebView* contents_web_view =
      browser_view()->GetContentsWebViewForTest();
  views::View* top_container = browser_view()->top_container();

  // Immersive fullscreen starts out disabled.
  ASSERT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());

  // The tabstrip and toolbar are not visible for hosted apps.
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());

  // The window header should be above the web contents.
  int header_height = GetBoundsInWidget(contents_web_view).y();

  ToggleFullscreen();
  EXPECT_TRUE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_TRUE(controller()->IsEnabled());
  EXPECT_FALSE(controller()->IsRevealed());

  // Entering immersive fullscreen should make the web contents flush with the
  // top of the widget. The popup browser type doesn't support tabstrip and
  // toolbar feature, thus invisible.
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());
  EXPECT_TRUE(top_container->GetVisibleBounds().IsEmpty());
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // Reveal the window header.
  AttemptReveal();

  // The tabstrip and toolbar should still be hidden and the web contents should
  // still be flush with the top of the screen.
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());
  EXPECT_EQ(0, GetBoundsInWidget(contents_web_view).y());

  // During an immersive reveal, the window header should be painted to the
  // TopContainerView. The TopContainerView should be flush with the top of the
  // widget and have |header_height|.
  gfx::Rect top_container_bounds_in_widget(GetBoundsInWidget(top_container));
  EXPECT_EQ(0, top_container_bounds_in_widget.y());
  EXPECT_EQ(header_height, top_container_bounds_in_widget.height());

  // Exit immersive fullscreen. The web contents should be back below the window
  // header.
  ToggleFullscreen();
  EXPECT_FALSE(browser_view()->GetWidget()->IsFullscreen());
  EXPECT_FALSE(controller()->IsEnabled());
  EXPECT_FALSE(tabstrip->visible());
  EXPECT_FALSE(toolbar->visible());
  EXPECT_EQ(header_height, GetBoundsInWidget(contents_web_view).y());
}

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

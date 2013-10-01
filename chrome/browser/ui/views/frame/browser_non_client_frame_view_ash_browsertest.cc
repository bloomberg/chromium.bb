// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

using views::Widget;

typedef InProcessBrowserTest BrowserNonClientFrameViewAshTest;

IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest, WindowHeader) {
  // We know we're using Views, so static cast.
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Restored window uses tall header.
  const int kWindowWidth = 300;
  const int kWindowHeight = 290;
  widget->SetBounds(gfx::Rect(10, 10, kWindowWidth, kWindowHeight));
  EXPECT_FALSE(frame_view->UseShortHeader());

  // Click on the top edge of a window hits the top edge resize handle.
  gfx::Point top_edge(kWindowWidth / 2, 0);
  EXPECT_EQ(HTTOP, frame_view->NonClientHitTest(top_edge));

  // Click just below the resize handle hits the caption.
  gfx::Point below_resize(kWindowWidth / 2, ash::kResizeInsideBoundsSize);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(below_resize));

  // Window at top of screen uses normal header.
  widget->SetBounds(gfx::Rect(10, 0, kWindowWidth, kWindowHeight));
  EXPECT_FALSE(frame_view->UseShortHeader());

  // Maximized window uses short header.
  widget->Maximize();
  EXPECT_TRUE(frame_view->UseShortHeader());

  // Click in the top edge of a maximized window now hits the client area,
  // because we want it to fall through to the tab strip and select a tab.
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(top_edge));

  // Popups tall header.
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  Widget* popup_widget =
      static_cast<BrowserView*>(popup->window())->GetWidget();
  BrowserNonClientFrameViewAsh* popup_frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          popup_widget->non_client_view()->frame_view());
  popup_widget->SetBounds(gfx::Rect(5, 5, 200, 200));
  EXPECT_FALSE(popup_frame_view->UseShortHeader());

  // Apps use tall header.
  Browser* app = CreateBrowserForApp("name", browser()->profile());
  Widget* app_widget =
      static_cast<BrowserView*>(app->window())->GetWidget();
  BrowserNonClientFrameViewAsh* app_frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          app_widget->non_client_view()->frame_view());
  app_widget->SetBounds(gfx::Rect(15, 15, 250, 250));
  EXPECT_FALSE(app_frame_view->UseShortHeader());
}

// Test that the frame view does not do any painting in non-immersive
// fullscreen.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest,
                       NonImmersiveFullscreen) {
  // We know we're using Views, so static cast.
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());

  // No painting should occur in non-immersive fullscreen. (We enter into tab
  // fullscreen here because tab fullscreen is non-immersive even when
  // ImmersiveFullscreenConfiguration::UseImmersiveFullscreen()) returns
  // true.
  {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously.
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()->fullscreen_controller()->ToggleFullscreenModeForTab(
        web_contents, true);
    waiter->Wait();
  }
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(frame_view->ShouldPaint());

  // The client view abuts top of the window.
  EXPECT_EQ(0, frame_view->NonClientTopBorderHeight());

  // The frame should be painted again when fullscreen is exited and the caption
  // buttons should be visible.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
}

IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest, ImmersiveFullscreen) {
  if (!ImmersiveFullscreenConfiguration::UseImmersiveFullscreen())
    return;

  // We know we're using Views, so static cast.
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  ImmersiveModeControllerAsh* immersive_mode_controller =
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller());
  immersive_mode_controller->DisableAnimationsForTest();
  immersive_mode_controller->SetForceHideTabIndicatorsForTest(true);

  // Immersive mode starts disabled.
  ASSERT_FALSE(widget->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());

  // Going fullscreen enables immersive mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());

  // An immersive reveal shows the buttons and the top of the frame.
  immersive_mode_controller->StartRevealForTest(true);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_TRUE(frame_view->UseShortHeader());
  EXPECT_FALSE(frame_view->UseImmersiveLightbarHeaderStyle());

  // End the reveal. As the header does not paint a light bar when the
  // top-of-window views are not revealed, nothing should be painted.
  immersive_mode_controller->SetMouseHoveredForTest(false);
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  EXPECT_FALSE(frame_view->ShouldPaint());

  // Repeat test but with the tab light bar visible when the top-of-window views
  // are not revealed.
  immersive_mode_controller->SetForceHideTabIndicatorsForTest(false);

  // Immersive reveal should have same behavior as before.
  immersive_mode_controller->StartRevealForTest(true);
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_TRUE(frame_view->UseShortHeader());
  EXPECT_FALSE(frame_view->UseImmersiveLightbarHeaderStyle());

  // Ending the reveal should hide the caption buttons and the header should
  // be in the lightbar style.
  immersive_mode_controller->SetMouseHoveredForTest(false);
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_FALSE(frame_view->caption_button_container_->visible());
  EXPECT_TRUE(frame_view->UseShortHeader());
  EXPECT_TRUE(frame_view->UseImmersiveLightbarHeaderStyle());

  // Exiting fullscreen exits immersive mode.
  browser_view->ExitFullscreen();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Exiting immersive mode makes controls and frame visible again.
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view->UseImmersiveLightbarHeaderStyle());
}

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/header_painter.h"
#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_test.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#endif  // defined(OS_CHROMEOS)

using views::Widget;

typedef InProcessBrowserTest BrowserNonClientFrameViewAshTest;

IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest, NonClientHitTest) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Click on the top edge of a restored window hits the top edge resize handle.
  const int kWindowWidth = 300;
  const int kWindowHeight = 290;
  widget->SetBounds(gfx::Rect(10, 10, kWindowWidth, kWindowHeight));
  gfx::Point top_edge(kWindowWidth / 2, 0);
  EXPECT_EQ(HTTOP, frame_view->NonClientHitTest(top_edge));

  // Click just below the resize handle hits the caption.
  gfx::Point below_resize(kWindowWidth / 2, ash::kResizeInsideBoundsSize);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(below_resize));

  // Click in the top edge of a maximized window now hits the client area,
  // because we want it to fall through to the tab strip and select a tab.
  widget->Maximize();
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(top_edge));
}

// Test that the frame view does not do any painting in non-immersive
// fullscreen.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest,
                       NonImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());

  // No painting should occur in non-immersive fullscreen. (We enter into tab
  // fullscreen here because tab fullscreen is non-immersive even on ChromeOS).
  {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously.
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents, GURL());
    waiter->Wait();
  }
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(frame_view->ShouldPaint());

  // The client view abuts top of the window.
  EXPECT_EQ(0, frame_view->GetBoundsForClientView().y());

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

// TODO(zturner): Change this to USE_ASH after fixing the test on Windows.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest, ImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  immersive_mode_controller->SetupForTest();

  // Immersive fullscreen starts disabled.
  ASSERT_FALSE(widget->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(Tab::GetImmersiveHeight(),
            frame_view->header_painter_->GetHeaderHeightForPainting());

  // Enter both browser fullscreen and tab fullscreen. Entering browser
  // fullscreen should enable immersive fullscreen.
  {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously.
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents, GURL());
    waiter->Wait();
  }
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());

  // An immersive reveal shows the buttons and the top of the frame.
  scoped_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view->UseImmersiveLightbarHeaderStyle());

  // End the reveal. When in both immersive browser fullscreen and tab
  // fullscreen, the tab lightbars should not be painted.
  revealed_lock.reset();
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  EXPECT_FALSE(frame_view->ShouldPaint());
  EXPECT_EQ(0, frame_view->header_painter_->GetHeaderHeightForPainting());

  // Repeat test but without tab fullscreen. The tab lightbars should now show
  // when the top-of-window views are not revealed.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->ExitFullscreenModeForTab(web_contents);
    waiter->Wait();
  }

  // Immersive reveal should have same behavior as before.
  revealed_lock.reset(immersive_mode_controller->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view->UseImmersiveLightbarHeaderStyle());
  EXPECT_LT(Tab::GetImmersiveHeight(),
            frame_view->header_painter_->GetHeaderHeightForPainting());

  // Ending the reveal should hide the caption buttons and the header should
  // be in the lightbar style.
  revealed_lock.reset();
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_FALSE(frame_view->caption_button_container_->visible());
  EXPECT_TRUE(frame_view->UseImmersiveLightbarHeaderStyle());
  EXPECT_EQ(Tab::GetImmersiveHeight(),
            frame_view->header_painter_->GetHeaderHeightForPainting());

  // Exiting immersive fullscreen should make the caption buttons and the frame
  // visible again.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser_view->ExitFullscreen();
    waiter->Wait();
  }
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view->UseImmersiveLightbarHeaderStyle());
  EXPECT_LT(Tab::GetImmersiveHeight(),
            frame_view->header_painter_->GetHeaderHeightForPainting());
}

// Tests that Avatar icon should show on the top left corner of the teleported
// browser window on ChromeOS.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest,
                       AvatarDisplayOnTeleportedWindow) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());
  aura::Window* window = browser()->window()->GetNativeWindow();

  EXPECT_FALSE(chrome::MultiUserWindowManager::ShouldShowAvatar(window));

  const std::string current_user =
      multi_user_util::GetUserIDFromProfile(browser()->profile());
  TestMultiUserWindowManager* manager =
      new TestMultiUserWindowManager(browser(), current_user);

  // Teleport the window to another desktop.
  const std::string user2 = "user2";
  manager->ShowWindowForUser(window, user2);
  EXPECT_TRUE(chrome::MultiUserWindowManager::ShouldShowAvatar(window));

  // Avatar should show on the top left corner of the teleported browser window.
  EXPECT_TRUE(frame_view->avatar_button());
}

// Hit Test for Avatar Menu Button on ChromeOS.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest,
                       AvatarMenuButtonHitTestOnChromeOS) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  gfx::Point avatar_center(profiles::kAvatarIconWidth / 2,
                           profiles::kAvatarIconHeight / 2);
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(avatar_center));

  const std::string current_user =
      multi_user_util::GetUserIDFromProfile(browser()->profile());
  TestMultiUserWindowManager* manager =
      new TestMultiUserWindowManager(browser(), current_user);

  // Teleport the window to another desktop.
  const std::string user2 = "user2";
  manager->ShowWindowForUser(browser()->window()->GetNativeWindow(), user2);
  // Clicking on the avatar icon should have same behaviour like clicking on
  // the caption area, i.e., allow the user to drag the browser window around.
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(avatar_center));
}

#endif  // defined(OS_CHROMEOS)

// Tests that FrameCaptionButtonContainer has been relaid out in response to
// maximize mode being toggled.
IN_PROC_BROWSER_TEST_F(BrowserNonClientFrameViewAshTest,
                       ToggleMaximizeModeRelayout) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  const gfx::Rect initial = frame_view->caption_button_container_->bounds();
  ash::Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  ash::FrameCaptionButtonContainerView::TestApi test(frame_view->
                                                     caption_button_container_);
  test.EndAnimations();
  const gfx::Rect during_maximize = frame_view->caption_button_container_->
      bounds();
  EXPECT_GT(initial.width(), during_maximize.width());
  ash::Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  test.EndAnimations();
  const gfx::Rect after_restore = frame_view->caption_button_container_->
      bounds();
  EXPECT_EQ(initial, after_restore);
}

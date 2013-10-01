// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#endif

class ZoomBubbleBrowserTest : public InProcessBrowserTest {
 public:
  ZoomBubbleBrowserTest() {}
  virtual ~ZoomBubbleBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ImmersiveFullscreenConfiguration::EnableImmersiveFullscreenForTest();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleBrowserTest);
};

// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
#define MAYBE_NonImmersiveFullscreen DISABLED_NonImmersiveFullscreen
#else
#define MAYBE_NonImmersiveFullscreen NonImmersiveFullscreen
#endif
// Test whether the zoom bubble is anchored and whether it is visible when in
// non-immersive fullscreen.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, MAYBE_NonImmersiveFullscreen) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();

  // The zoom bubble should be anchored when not in fullscreen.
  ZoomBubbleView::ShowBubble(web_contents, true);
  ASSERT_TRUE(ZoomBubbleView::IsShowing());
  const ZoomBubbleView* zoom_bubble = ZoomBubbleView::GetZoomBubbleForTest();
  EXPECT_TRUE(zoom_bubble->GetAnchorView());

  // Entering fullscreen should close the bubble. (We enter into tab fullscreen
  // here because tab fullscreen is non-immersive even when
  // ImmersiveFullscreenConfiguration::UseImmersiveFullscreen()) returns
  // true.
  {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously. Wait for the
    // notification before testing the zoom bubble visibility.
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()->fullscreen_controller()->ToggleFullscreenModeForTab(
        web_contents, true);
    waiter->Wait();
  }
  ASSERT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(ZoomBubbleView::IsShowing());

  // The bubble should not be anchored when it is shown in non-immersive
  // fullscreen.
  ZoomBubbleView::ShowBubble(web_contents, true);
  ASSERT_TRUE(ZoomBubbleView::IsShowing());
  zoom_bubble = ZoomBubbleView::GetZoomBubbleForTest();
  EXPECT_FALSE(zoom_bubble->GetAnchorView());

  // Exit fullscreen before ending the test for the sake of sanity.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }
}

// Immersive fullscreen is CrOS only for now.
#if defined(OS_CHROMEOS)
// Test whether the zoom bubble is anchored and whether it is visible when in
// immersive fullscreen.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, ImmersiveFullscreen) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();

  ASSERT_TRUE(ImmersiveFullscreenConfiguration::UseImmersiveFullscreen());
  ImmersiveModeControllerAsh* immersive_controller =
      static_cast<ImmersiveModeControllerAsh*>(
          browser_view->immersive_mode_controller());
  immersive_controller->DisableAnimationsForTest();

  // Move the mouse out of the way.
  immersive_controller->SetMouseHoveredForTest(false);

  // Enter immersive fullscreen.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }
  ASSERT_TRUE(immersive_controller->IsEnabled());
  ASSERT_FALSE(immersive_controller->IsRevealed());

  // The zoom bubble should not be anchored when it is shown in immersive
  // fullscreen and the top-of-window views are not revealed.
  ZoomBubbleView::ShowBubble(web_contents, true);
  ASSERT_TRUE(ZoomBubbleView::IsShowing());
  const ZoomBubbleView* zoom_bubble = ZoomBubbleView::GetZoomBubbleForTest();
  EXPECT_FALSE(zoom_bubble->GetAnchorView());

  // An immersive reveal should hide the zoom bubble.
  scoped_ptr<ImmersiveRevealedLock> immersive_reveal_lock(
      immersive_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  ASSERT_TRUE(immersive_controller->IsRevealed());
  EXPECT_FALSE(ZoomBubbleView::IsShowing());

  // The zoom bubble should be anchored when it is shown in immersive fullscreen
  // and the top-of-window views are revealed.
  ZoomBubbleView::ShowBubble(web_contents, true);
  ASSERT_TRUE(ZoomBubbleView::IsShowing());
  zoom_bubble = ZoomBubbleView::GetZoomBubbleForTest();
  EXPECT_TRUE(zoom_bubble->GetAnchorView());

  // The top-of-window views should not hide till the zoom bubble hides. (It
  // would be weird if the view to which the zoom bubble is anchored hid while
  // the zoom bubble was still visible.)
  immersive_reveal_lock.reset();
  EXPECT_TRUE(immersive_controller->IsRevealed());
  ZoomBubbleView::CloseBubble();
  // The zoom bubble is deleted on a task.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(immersive_controller->IsRevealed());

  // Exit fullscreen before ending the test for the sake of sanity.
  {
    scoped_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    chrome::ToggleFullscreenMode(browser());
    waiter->Wait();
  }
}
#endif  // OS_CHROMEOS

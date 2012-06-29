// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen_controller_test.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

class FullscreenControllerInteractiveTest
    : public FullscreenControllerTest {
 protected:
  // IsMouseLocked verifies that the FullscreenController state believes
  // the mouse is locked. This is possible only for tests that initiate
  // mouse lock from a renderer process, and uses logic that tests that the
  // browser has focus. Thus, this can only be used in interactive ui tests
  // and not on sharded tests.
  bool IsMouseLocked() {
    // Verify that IsMouseLocked is consistent between the
    // Fullscreen Controller and the Render View Host View.
    EXPECT_TRUE(browser()->IsMouseLocked() ==
                browser()->GetActiveWebContents()->
                    GetRenderViewHost()->GetView()->IsMouseLocked());
    return browser()->IsMouseLocked();
  }
};

// Tests mouse lock can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest, EscapingMouseLock) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Request to lock the mouse.
  {
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_1, false, false, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
  }
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Escape, no prompts should remain.
  SendEscapeToFullscreenController();
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());

  // Request to lock the mouse.
  {
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_1, false, false, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
  }
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Accept mouse lock, confirm it and that there is no prompt.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenForTabOrPending());
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());

  // Escape, confirm we are out of mouse lock with no prompts.
  SendEscapeToFullscreenController();
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenForTabOrPending());
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());
}

// Times out sometimes on Linux. http://crbug.com/135115
#if defined(OS_LINUX)
#define MAYBE_EscapingMouseLockAndFullscreen \
    DISABLED_EscapingMouseLockAndFullscreen
#else
#define MAYBE_EscapingMouseLockAndFullscreen EscapingMouseLockAndFullscreen
#endif
// Tests mouse lock and fullscreen modes can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_EscapingMouseLockAndFullscreen) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer;
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_B, false, true, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
    fullscreen_observer.Wait();
  }
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Escape, no prompts should remain.
  {
    FullscreenNotificationObserver fullscreen_observer;
    SendEscapeToFullscreenController();
    fullscreen_observer.Wait();
  }
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer;
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_B, false, true, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
    fullscreen_observer.Wait();
  }
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Accept both, confirm mouse lock and fullscreen and no prompts.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenForTabOrPending());
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());

  // Escape, confirm we are out of mouse lock and fullscreen with no prompts.
  {
    FullscreenNotificationObserver fullscreen_observer;
    SendEscapeToFullscreenController();
    fullscreen_observer.Wait();
  }
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenForTabOrPending());
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());
}

// Tests mouse lock then fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MouseLockThenFullscreen) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Lock the mouse without a user gesture, expect no response.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_D, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_FALSE(IsFullscreenBubbleDisplayed());
  ASSERT_FALSE(IsMouseLocked());

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());

  // Accept mouse lock.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenBubbleDisplayingButtons());

  // Enter fullscreen mode, mouse lock should be dropped to present buttons.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());

  // Request mouse lock also, expect fullscreen and mouse lock buttons.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());

  // Accept fullscreen and mouse lock.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenForTabOrPending());
  ASSERT_FALSE(IsFullscreenBubbleDisplayingButtons());
}

// Times out sometimes on Linux. http://crbug.com/135115
#if defined(OS_LINUX)
#define MAYBE_MouseLockAndFullscreen DISABLED_MouseLockAndFullscreen
#else
#define MAYBE_MouseLockAndFullscreen MouseLockAndFullscreen
#endif
// Tests mouse lock then fullscreen in same request.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_MouseLockAndFullscreen) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer;
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_B, false, true, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
    fullscreen_observer.Wait();
  }
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenForTabOrPending());

  // Deny both first, to make sure we can.
  {
    FullscreenNotificationObserver fullscreen_observer;
    DenyCurrentFullscreenOrMouseLockRequest();
    fullscreen_observer.Wait();
  }
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenForTabOrPending());
  ASSERT_FALSE(IsFullscreenPermissionRequested());

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer;
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_B, false, true, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
    fullscreen_observer.Wait();
  }
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenForTabOrPending());

  // Accept both, confirm they are enabled and there is no prompt.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenForTabOrPending());
  ASSERT_FALSE(IsFullscreenPermissionRequested());
}

// Tests mouse lock can be exited and re-entered by an application silently
// with no UI distraction for users.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MouseLockSilentAfterTargetUnlock) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());

  // Accept mouse lock.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  // Unlock the mouse from target, make sure it's unlocked.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_U, false, false, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Lock mouse again, make sure it works with no bubble.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_1, false, false, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Unlock the mouse again by target.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_U, false, false, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
  ASSERT_FALSE(IsMouseLocked());

  // Lock from target, not user gesture, make sure it works.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_D, false, false, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Unlock by escape.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
          browser(), ui::VKEY_ESCAPE, false, false, false, false,
          chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
          content::NotificationService::AllSources()));
  ASSERT_FALSE(IsMouseLocked());

  // Lock the mouse with a user gesture, make sure we see bubble again.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsMouseLocked());
}

// Tests mouse lock is exited on page navigation.
// (Similar to fullscreen version in FullscreenControllerTest)
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsMouseLockOnNavigation) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());

  // Accept mouse lock.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_FALSE(IsMouseLocked());
}

// Tests mouse lock is exited when navigating back.
// (Similar to fullscreen version in FullscreenControllerTest)
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabExitsMouseLockOnGoBack) {
  ASSERT_TRUE(test_server()->Start());

  // Navigate twice to provide a place to go back to.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());

  // Accept mouse lock.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());

  GoBack();

  ASSERT_FALSE(IsMouseLocked());
}

// Tests mouse lock is not exited on sub frame navigation.
// (Similar to fullscreen version in FullscreenControllerTest)
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       TestTabDoesntExitMouseLockOnSubFrameNavigation) {
  ASSERT_TRUE(test_server()->Start());

  // Create URLs for test page and test page with #fragment.
  GURL url(test_server()->GetURL(kFullscreenMouseLockHTML));
  GURL url_with_fragment(url.spec() + "#fragment");

  // Navigate to test page.
  ui_test_utils::NavigateToURL(browser(), url);

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  ASSERT_FALSE(IsMouseLocked());

  // Accept mouse lock.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());

  // Navigate to url with fragment. Mouse lock should persist.
  ui_test_utils::NavigateToURL(browser(), url_with_fragment);
  ASSERT_TRUE(IsMouseLocked());
}

// Tests Mouse Lock and Fullscreen are exited upon reload.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       ReloadExitsMouseLockAndFullscreen) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsMouseLockPermissionRequested());

  // Request mouse lock.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Reload. Mouse lock request should be cleared.
  {
    MouseLockNotificationObserver mouselock_observer;
    Reload();
    mouselock_observer.Wait();
    ASSERT_FALSE(IsMouseLockPermissionRequested());
  }

  // Request mouse lock.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Accept mouse lock.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  // Reload. Mouse should be unlocked.
  {
    MouseLockNotificationObserver mouselock_observer;
    Reload();
    mouselock_observer.Wait();
    ASSERT_FALSE(IsMouseLocked());
  }

  // Request to lock the mouse and enter fullscreen.
  {
    FullscreenNotificationObserver fullscreen_observer;
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_B, false, true, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
    fullscreen_observer.Wait();
  }

  // We are fullscreen.
  ASSERT_TRUE(IsFullscreenForTabOrPending());

  // Reload. Mouse should be unlocked and fullscreen exited.
  {
    FullscreenNotificationObserver fullscreen_observer;
    Reload();
    fullscreen_observer.Wait();
    ASSERT_FALSE(IsMouseLocked());
    ASSERT_FALSE(IsFullscreenForTabOrPending());
  }
}

// Fails sometimes on Linux. http://crbug.com/135115
#if defined(OS_LINUX)
#define MAYBE_ToggleFullscreenModeForTab DISABLED_ToggleFullscreenModeForTab
#else
#define MAYBE_ToggleFullscreenModeForTab ToggleFullscreenModeForTab
#endif
// Tests ToggleFullscreenModeForTab always causes window to change.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_ToggleFullscreenModeForTab) {
  // Most fullscreen tests run sharded in fullscreen_controller_browsertest.cc
  // but flakiness required a while loop in
  // FullscreenControllerTest::ToggleTabFullscreen. This test verifies that
  // when running serially there is no flakiness.
  // This test reproduces the same flow as
  // FullscreenControllerBrowserTest::TestFullscreenMouseLockContentSettings.
  // http://crbug.com/133831

  GURL url = test_server()->GetURL("simple.html");
  AddTabAtIndexAndWait(0, url, content::PAGE_TRANSITION_TYPED);

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(false));
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::kAboutBlankURL;
using content::WebContents;
using content::PAGE_TRANSITION_TYPED;

namespace {

const base::FilePath::CharType* kSimpleFile = FILE_PATH_LITERAL("simple.html");

}  // namespace

class FullscreenControllerInteractiveTest
    : public FullscreenControllerTest {
 protected:

  // Tests that actually make the browser fullscreen have been flaky when
  // run sharded, and so are restricted here to interactive ui tests.
  void ToggleTabFullscreen(bool enter_fullscreen);
  void ToggleTabFullscreenNoRetries(bool enter_fullscreen);
  void ToggleBrowserFullscreen(bool enter_fullscreen);

  // IsMouseLocked verifies that the FullscreenController state believes
  // the mouse is locked. This is possible only for tests that initiate
  // mouse lock from a renderer process, and uses logic that tests that the
  // browser has focus. Thus, this can only be used in interactive ui tests
  // and not on sharded tests.
  bool IsMouseLocked() {
    // Verify that IsMouseLocked is consistent between the
    // Fullscreen Controller and the Render View Host View.
    EXPECT_TRUE(browser()->IsMouseLocked() ==
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetRenderViewHost()->GetView()->IsMouseLocked());
    return browser()->IsMouseLocked();
  }

  void TestFullscreenMouseLockContentSettings();

 private:
   void ToggleTabFullscreen_Internal(bool enter_fullscreen,
                                     bool retry_until_success);
};

void FullscreenControllerInteractiveTest::ToggleTabFullscreen(
    bool enter_fullscreen) {
  ToggleTabFullscreen_Internal(enter_fullscreen, true);
}

// |ToggleTabFullscreen| should not need to tolerate the transition failing.
// Most fullscreen tests run sharded in fullscreen_controller_browsertest.cc
// and some flakiness has occurred when calling |ToggleTabFullscreen|, so that
// method has been made robust by retrying if the transition fails.
// The root cause of that flakiness should still be tracked down, see
// http://crbug.com/133831. In the mean time, this method
// allows a fullscreen_controller_interactive_browsertest.cc test to verify
// that when running serially there is no flakiness in the transition.
void FullscreenControllerInteractiveTest::ToggleTabFullscreenNoRetries(
    bool enter_fullscreen) {
  ToggleTabFullscreen_Internal(enter_fullscreen, false);
}

void FullscreenControllerInteractiveTest::ToggleBrowserFullscreen(
    bool enter_fullscreen) {
  ASSERT_EQ(browser()->window()->IsFullscreen(), !enter_fullscreen);
  FullscreenNotificationObserver fullscreen_observer;

  chrome::ToggleFullscreenMode(browser());

  fullscreen_observer.Wait();
  ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
  ASSERT_EQ(IsFullscreenForBrowser(), enter_fullscreen);
}

// Helper method to be called by multiple tests.
// Tests Fullscreen and Mouse Lock with varying content settings ALLOW & BLOCK.
void
FullscreenControllerInteractiveTest::TestFullscreenMouseLockContentSettings() {
  GURL url = test_server()->GetURL("simple.html");
  AddTabAtIndex(0, url, PAGE_TRANSITION_TYPED);

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));

  // Add content setting to ALLOW fullscreen.
  HostContentSettingsMap* settings_map =
      browser()->profile()->GetHostContentSettingsMap();
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(url);
  settings_map->SetContentSetting(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_FULLSCREEN, std::string(),
      CONTENT_SETTING_ALLOW);

  // Now, fullscreen should not prompt for permission.
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_FALSE(IsFullscreenPermissionRequested());

  // Leaving tab in fullscreen, now test mouse lock ALLOW:

  // Validate that mouse lock defaults to asking permision.
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  LostMouseLock();

  // Add content setting to ALLOW mouse lock.
  settings_map->SetContentSetting(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string(),
      CONTENT_SETTING_ALLOW);

  // Now, mouse lock should not prompt for permission.
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  RequestToLockMouse(true, false);
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  LostMouseLock();

  // Leaving tab in fullscreen, now test mouse lock BLOCK:

  // Add content setting to BLOCK mouse lock.
  settings_map->SetContentSetting(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string(),
      CONTENT_SETTING_BLOCK);

  // Now, mouse lock should not be pending.
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  RequestToLockMouse(true, false);
  ASSERT_FALSE(IsMouseLockPermissionRequested());
}

void FullscreenControllerInteractiveTest::ToggleTabFullscreen_Internal(
    bool enter_fullscreen, bool retry_until_success) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  do {
    FullscreenNotificationObserver fullscreen_observer;
    browser()->ToggleFullscreenModeForTab(tab, enter_fullscreen);
    fullscreen_observer.Wait();
    // Repeat ToggleFullscreenModeForTab until the correct state is entered.
    // This addresses flakiness on test bots running many fullscreen
    // tests in parallel.
  } while (retry_until_success &&
           !IsFullscreenForBrowser() &&
           browser()->window()->IsFullscreen() != enter_fullscreen);
  ASSERT_EQ(IsFullscreenForTabOrPending(), enter_fullscreen);
  if (!IsFullscreenForBrowser())
    ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
}

// Tests ///////////////////////////////////////////////////////////////////////

// Tests that while in fullscreen creating a new tab will exit fullscreen.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_TestNewTabExitsFullscreen) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  {
    FullscreenNotificationObserver fullscreen_observer;
    AddTabAtIndex(1, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

// Tests a tab exiting fullscreen will bring the browser out of fullscreen.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_TestTabExitsItselfFromFullscreen) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests entering fullscreen and then requesting mouse lock results in
// buttons for the user, and that after confirming the buttons are dismissed.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_TestFullscreenBubbleMouseLockState) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);
  AddTabAtIndex(1, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  // Request mouse lock and verify the bubble is waiting for user confirmation.
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Accept mouse lock and verify bubble no longer shows confirmation buttons.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_FALSE(IsFullscreenBubbleDisplayingButtons());
}

// Tests fullscreen and Mouse Lock with varying content settings ALLOW & BLOCK.
// http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_FullscreenMouseLockContentSettings) {
  TestFullscreenMouseLockContentSettings();
}

// Tests fullscreen and Mouse Lock with varying content settings ALLOW & BLOCK,
// but with the browser initiated in fullscreen mode first.
// Test is flaky: http://crbug.com/103912, http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_BrowserFullscreenMouseLockContentSettings) {
  // Enter browser fullscreen first.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));
  TestFullscreenMouseLockContentSettings();
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(false));
}

// Tests Fullscreen entered in Browser, then Tab mode, then exited via Browser.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_BrowserFullscreenExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter tab fullscreen.
  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  // Exit browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(false));
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests Browser Fullscreen remains active after Tab mode entered and exited.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_BrowserFullscreenAfterTabFSExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter and then exit tab fullscreen.
  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));

  // Verify browser fullscreen still active.
  ASSERT_TRUE(IsFullscreenForBrowser());
}

// Tests fullscreen entered without permision prompt for file:// urls.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_FullscreenFileURL) {
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kSimpleFile)));

  // Validate that going fullscreen for a file does not ask permision.
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests fullscreen is exited on page navigation.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_TestTabExitsFullscreenOnNavigation) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests fullscreen is exited when navigating back.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_TestTabExitsFullscreenOnGoBack) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  GoBack();

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests fullscreen is not exited on sub frame navigation.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerInteractiveTest,
    DISABLED_TestTabDoesntExitFullscreenOnSubFrameNavigation) {
  ASSERT_TRUE(test_server()->Start());

  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kSimpleFile)));
  GURL url_with_fragment(url.spec() + "#fragment");

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ui_test_utils::NavigateToURL(browser(), url_with_fragment);
  ASSERT_TRUE(IsFullscreenForTabOrPending());
}

// Tests tab fullscreen exits, but browser fullscreen remains, on navigation.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerInteractiveTest,
    DISABLED_TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  GoBack();

  ASSERT_TRUE(IsFullscreenForBrowser());
  ASSERT_FALSE(IsFullscreenForTabOrPending());
}

#if defined(OS_MACOSX)
// http://crbug.com/100467
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerTest, DISABLED_TabEntersPresentationModeFromWindowed) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(0, GURL(kAboutBlankURL), PAGE_TRANSITION_TYPED);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    FullscreenNotificationObserver fullscreen_observer;
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    EXPECT_FALSE(browser()->window()->IsFullscreenWithChrome());
    EXPECT_FALSE(browser()->window()->IsFullscreenWithoutChrome());
    browser()->ToggleFullscreenModeForTab(tab, true);
    fullscreen_observer.Wait();
    EXPECT_TRUE(browser()->window()->IsFullscreen());
    EXPECT_FALSE(browser()->window()->IsFullscreenWithChrome());
    EXPECT_TRUE(browser()->window()->IsFullscreenWithoutChrome());
  }

  {
    FullscreenNotificationObserver fullscreen_observer;
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    EXPECT_FALSE(browser()->window()->IsFullscreenWithChrome());
    EXPECT_FALSE(browser()->window()->IsFullscreenWithoutChrome());
  }

  if (chrome::mac::SupportsSystemFullscreen()) {
    // Test that tab fullscreen mode doesn't make presentation mode the default
    // on Lion.
    FullscreenNotificationObserver fullscreen_observer;
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
    EXPECT_TRUE(browser()->window()->IsFullscreen());
    EXPECT_TRUE(browser()->window()->IsFullscreenWithChrome());
    EXPECT_FALSE(browser()->window()->IsFullscreenWithoutChrome());
  }
}
#endif

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

// Tests mouse lock and fullscreen modes can be escaped with ESC key.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_EscapingMouseLockAndFullscreen) {
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
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_MouseLockThenFullscreen) {
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
// Mac: http://crbug.com/103912
// Windows: Failing flakily on try jobs also.
// Tests mouse lock then fullscreen in same request.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_MouseLockAndFullscreen) {
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

// Tests mouse lock and fullscreen for the privileged fullscreen case (e.g.,
// embedded flash fullscreen, since the Flash plugin handles user permissions
// requests itself).
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_PrivilegedMouseLockAndFullscreen) {
  ASSERT_TRUE(test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  SetPrivilegedFullscreen(true);

  // Request to lock the mouse and enter fullscreen.
  FullscreenNotificationObserver fullscreen_observer;
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_B, false, true, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  fullscreen_observer.Wait();

  // Confirm they are enabled and there is no prompt.
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenForTabOrPending());
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

#if defined(OS_WIN)
// These tests are very flaky on Vista.
// http://crbug.com/158762
#define MAYBE_TestTabExitsMouseLockOnNavigation \
    DISABLED_TestTabExitsMouseLockOnNavigation
#define MAYBE_TestTabExitsMouseLockOnGoBack \
    DISABLED_TestTabExitsMouseLockOnGoBack
#else
#define MAYBE_TestTabExitsMouseLockOnNavigation \
    TestTabExitsMouseLockOnNavigation
#define MAYBE_TestTabExitsMouseLockOnGoBack \
    TestTabExitsMouseLockOnGoBack
#endif

// Tests mouse lock is exited on page navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsMouseLockOnNavigation) {
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
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsMouseLockOnGoBack) {
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
// http://crbug.com/137486
// mac: http://crbug.com/103912
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_ReloadExitsMouseLockAndFullscreen) {
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

// Tests ToggleFullscreenModeForTab always causes window to change.
// Test is flaky: http://crbug.com/146006
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       DISABLED_ToggleFullscreenModeForTab) {
  // Most fullscreen tests run sharded in fullscreen_controller_browsertest.cc
  // but flakiness required a while loop in
  // FullscreenControllerTest::ToggleTabFullscreen. This test verifies that
  // when running serially there is no flakiness.
  // This test reproduces the same flow as
  // TestFullscreenMouseLockContentSettings.
  // http://crbug.com/133831

  GURL url = test_server()->GetURL("simple.html");
  AddTabAtIndex(0, url, PAGE_TRANSITION_TYPED);

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(false));
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using url::kAboutBlankURL;
using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;

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
                browser()
                    ->tab_strip_model()
                    ->GetActiveWebContents()
                    ->GetRenderViewHost()
                    ->GetWidget()
                    ->GetView()
                    ->IsMouseLocked());
    return browser()->IsMouseLocked();
  }

 private:
   void ToggleTabFullscreen_Internal(bool enter_fullscreen,
                                     bool retry_until_success);
};

// Allow the test harness to pick the non-simplified fullscreen UI, which
// prompts with an interactive dialog rather than just notifying the user how to
// exit fullscreen. TODO(tapted): Remove this when "simplified" is the only way.
enum TestType {
  TEST_TYPE_START,
  PROMPTING = TEST_TYPE_START,
  SIMPLIFIED,
  TEST_TYPE_END,
};

class ParamaterizedFullscreenControllerInteractiveTest
    : public FullscreenControllerInteractiveTest,
      public ::testing::WithParamInterface<int> {
 public:
  ParamaterizedFullscreenControllerInteractiveTest() {
    switch (GetParam()) {
      case PROMPTING:
        scoped_feature_list_.InitAndDisableFeature(
            features::kSimplifiedFullscreenUI);
        break;
      case SIMPLIFIED:
        scoped_feature_list_.InitAndEnableFeature(
            features::kSimplifiedFullscreenUI);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ParamaterizedFullscreenControllerInteractiveTest);
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

void FullscreenControllerInteractiveTest::ToggleTabFullscreen_Internal(
    bool enter_fullscreen, bool retry_until_success) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  do {
    FullscreenNotificationObserver fullscreen_observer;
    if (enter_fullscreen)
      browser()->EnterFullscreenModeForTab(tab, GURL());
    else
      browser()->ExitFullscreenModeForTab(tab);
    fullscreen_observer.Wait();
    // Repeat ToggleFullscreenModeForTab until the correct state is entered.
    // This addresses flakiness on test bots running many fullscreen
    // tests in parallel.
  } while (retry_until_success &&
           !IsFullscreenForBrowser() &&
           browser()->window()->IsFullscreen() != enter_fullscreen);
  ASSERT_EQ(IsWindowFullscreenForTabOrPending(), enter_fullscreen);
  if (!IsFullscreenForBrowser())
    ASSERT_EQ(browser()->window()->IsFullscreen(), enter_fullscreen);
}

// Tests ///////////////////////////////////////////////////////////////////////

// Tests that while in fullscreen creating a new tab will exit fullscreen.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_TestNewTabExitsFullscreen TestNewTabExitsFullscreen
#else
#define MAYBE_TestNewTabExitsFullscreen DISABLED_TestNewTabExitsFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestNewTabExitsFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());

  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  {
    FullscreenNotificationObserver fullscreen_observer;
    AddTabAtIndex(1, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

// Tests a tab exiting fullscreen will bring the browser out of fullscreen.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_TestTabExitsItselfFromFullscreen TestTabExitsItselfFromFullscreen
#else
#define MAYBE_TestTabExitsItselfFromFullscreen \
  DISABLED_TestTabExitsItselfFromFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsItselfFromFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());

  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests Fullscreen entered in Browser, then Tab mode, then exited via Browser.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_BrowserFullscreenExit BrowserFullscreenExit
#else
#define MAYBE_BrowserFullscreenExit DISABLED_BrowserFullscreenExit
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_BrowserFullscreenExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter tab fullscreen.
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  // Exit browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(false));
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests Browser Fullscreen remains active after Tab mode entered and exited.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_BrowserFullscreenAfterTabFSExit BrowserFullscreenAfterTabFSExit
#else
#define MAYBE_BrowserFullscreenAfterTabFSExit \
  DISABLED_BrowserFullscreenAfterTabFSExit
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_BrowserFullscreenAfterTabFSExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter and then exit tab fullscreen.
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));

  // Verify browser fullscreen still active.
  ASSERT_TRUE(IsFullscreenForBrowser());
}

// Tests fullscreen entered without permision prompt for file:// urls.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_FullscreenFileURL FullscreenFileURL
#else
#define MAYBE_FullscreenFileURL DISABLED_FullscreenFileURL
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_FullscreenFileURL) {
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kSimpleFile)));

  // Validate that going fullscreen for a file does not ask permision.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(false));
}

// Tests fullscreen is exited on page navigation.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_TestTabExitsFullscreenOnNavigation \
  TestTabExitsFullscreenOnNavigation
#else
#define MAYBE_TestTabExitsFullscreenOnNavigation \
  DISABLED_TestTabExitsFullscreenOnNavigation
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsFullscreenOnNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests fullscreen is exited when navigating back.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_TestTabExitsFullscreenOnGoBack TestTabExitsFullscreenOnGoBack
#else
#define MAYBE_TestTabExitsFullscreenOnGoBack \
  DISABLED_TestTabExitsFullscreenOnGoBack
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsFullscreenOnGoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  GoBack();

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests fullscreen is not exited on sub frame navigation.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_TestTabDoesntExitFullscreenOnSubFrameNavigation \
  TestTabDoesntExitFullscreenOnSubFrameNavigation
#else
#define MAYBE_TestTabDoesntExitFullscreenOnSubFrameNavigation \
  DISABLED_TestTabDoesntExitFullscreenOnSubFrameNavigation
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabDoesntExitFullscreenOnSubFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kSimpleFile)));
  GURL url_with_fragment(url.spec() + "#fragment");

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ui_test_utils::NavigateToURL(browser(), url_with_fragment);
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Tests tab fullscreen exits, but browser fullscreen remains, on navigation.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks \
  TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks
#else
#define MAYBE_TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks \
  DISABLED_TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks
#endif
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerInteractiveTest,
    MAYBE_TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));

  GoBack();

  ASSERT_TRUE(IsFullscreenForBrowser());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

#if defined(OS_MACOSX)
// http://crbug.com/100467
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerTest, DISABLED_TabEntersPresentationModeFromWindowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  AddTabAtIndex(0, GURL(url::kAboutBlankURL), PAGE_TRANSITION_TYPED);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    FullscreenNotificationObserver fullscreen_observer;
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    browser()->EnterFullscreenModeForTab(tab, GURL());
    fullscreen_observer.Wait();
    EXPECT_TRUE(browser()->window()->IsFullscreen());
  }

  {
    FullscreenNotificationObserver fullscreen_observer;
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
    EXPECT_FALSE(browser()->window()->IsFullscreen());
  }

  {
    // Test that tab fullscreen mode doesn't make presentation mode the default
    // on Lion.
    FullscreenNotificationObserver fullscreen_observer;
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
    EXPECT_TRUE(browser()->window()->IsFullscreen());
  }
}
#endif

// Tests mouse lock can be escaped with ESC key.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest, EscapingMouseLock) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Request to lock the mouse.
  {
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_1, false, false, false, false,
        chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
        content::NotificationService::AllSources()));
  }

  ASSERT_TRUE(IsMouseLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());

  // Escape, confirm we are out of mouse lock with no prompts.
  SendEscapeToFullscreenController();
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests mouse lock and fullscreen modes can be escaped with ESC key.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_EscapingMouseLockAndFullscreen EscapingMouseLockAndFullscreen
#else
#define MAYBE_EscapingMouseLockAndFullscreen \
  DISABLED_EscapingMouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_EscapingMouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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

  // Escape, no prompts should remain.
  {
    FullscreenNotificationObserver fullscreen_observer;
    SendEscapeToFullscreenController();
    fullscreen_observer.Wait();
  }
  ASSERT_FALSE(IsMouseLocked());
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

// Tests mouse lock then fullscreen.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_MouseLockThenFullscreen MouseLockThenFullscreen
#else
#define MAYBE_MouseLockThenFullscreen DISABLED_MouseLockThenFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_MouseLockThenFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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
  ASSERT_TRUE(IsMouseLocked());

  // Enter fullscreen mode, mouse should remain locked.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(true));
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Times out sometimes on Linux. http://crbug.com/135115
// Mac: http://crbug.com/103912
// Windows: Failing flakily on try jobs also.
// Tests mouse lock then fullscreen in same request.
#if defined(OS_WIN)
#define MAYBE_MouseLockAndFullscreen MouseLockAndFullscreen
#else
#define MAYBE_MouseLockAndFullscreen DISABLED_MouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_MouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Tests mouse lock and fullscreen for the privileged fullscreen case (e.g.,
// embedded flash fullscreen, since the Flash plugin handles user permissions
// requests itself).
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_PrivilegedMouseLockAndFullscreen PrivilegedMouseLockAndFullscreen
#else
#define MAYBE_PrivilegedMouseLockAndFullscreen \
  DISABLED_PrivilegedMouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_PrivilegedMouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

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
  ASSERT_FALSE(IsFullscreenBubbleDisplayed());
  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
}

// Flaky on Linux, CrOS: http://crbug.com/159000
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_MouseLockSilentAfterTargetUnlock \
  DISABLED_MouseLockSilentAfterTargetUnlock
#elif defined(OS_WIN)
// Flaky on Windows; see https://crbug.com/791539.
#define MAYBE_MouseLockSilentAfterTargetUnlock \
  DISABLED_MouseLockSilentAfterTargetUnlock
#else
#define MAYBE_MouseLockSilentAfterTargetUnlock MouseLockSilentAfterTargetUnlock
#endif

// Tests mouse lock can be exited and re-entered by an application silently
// with no UI distraction for users.
IN_PROC_BROWSER_TEST_P(ParamaterizedFullscreenControllerInteractiveTest,
                       MAYBE_MouseLockSilentAfterTargetUnlock) {
  SetWebContentsGrantedSilentMouseLockPermission();
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  ASSERT_FALSE(IsFullscreenBubbleDisplayed());

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
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

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// These are flaky on linux_aura.
// http://crbug.com/163931
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
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_FALSE(IsMouseLocked());
}

// Tests mouse lock is exited when navigating back.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabExitsMouseLockOnGoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate twice to provide a place to go back to.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  GoBack();

  ASSERT_FALSE(IsMouseLocked());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_TestTabDoesntExitMouseLockOnSubFrameNavigation \
  DISABLED_TestTabDoesntExitMouseLockOnSubFrameNavigation
#else
#define MAYBE_TestTabDoesntExitMouseLockOnSubFrameNavigation \
  TestTabDoesntExitMouseLockOnSubFrameNavigation
#endif

// Tests mouse lock is not exited on sub frame navigation.
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_TestTabDoesntExitMouseLockOnSubFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create URLs for test page and test page with #fragment.
  GURL url(embedded_test_server()->GetURL(kFullscreenMouseLockHTML));
  GURL url_with_fragment(url.spec() + "#fragment");

  // Navigate to test page.
  ui_test_utils::NavigateToURL(browser(), url);

  // Lock the mouse with a user gesture.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  ASSERT_TRUE(IsMouseLocked());

  // Navigate to url with fragment. Mouse lock should persist.
  ui_test_utils::NavigateToURL(browser(), url_with_fragment);
  ASSERT_TRUE(IsMouseLocked());
}

// Tests Mouse Lock and Fullscreen are exited upon reload.
// http://crbug.com/137486
// mac: http://crbug.com/103912
#if defined(OS_WIN)
#define MAYBE_ReloadExitsMouseLockAndFullscreen \
  ReloadExitsMouseLockAndFullscreen
#else
#define MAYBE_ReloadExitsMouseLockAndFullscreen \
  DISABLED_ReloadExitsMouseLockAndFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_ReloadExitsMouseLockAndFullscreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kFullscreenMouseLockHTML));

  // Request mouse lock.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_1, false, false, false, false,
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::NotificationService::AllSources()));

  ASSERT_TRUE(IsMouseLocked());
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());

  // Reload. Mouse lock request should be cleared.
  {
    MouseLockNotificationObserver mouselock_observer;
    Reload();
    mouselock_observer.Wait();
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
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());

  // Reload. Mouse should be unlocked and fullscreen exited.
  {
    FullscreenNotificationObserver fullscreen_observer;
    Reload();
    fullscreen_observer.Wait();
    ASSERT_FALSE(IsMouseLocked());
    ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
  }
}

// Tests ToggleFullscreenModeForTab always causes window to change.
// Test is flaky: http://crbug.com/146006
#if defined(OS_WIN)
#define MAYBE_ToggleFullscreenModeForTab ToggleFullscreenModeForTab
#else
#define MAYBE_ToggleFullscreenModeForTab DISABLED_ToggleFullscreenModeForTab
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControllerInteractiveTest,
                       MAYBE_ToggleFullscreenModeForTab) {
  // Most fullscreen tests run sharded in fullscreen_controller_browsertest.cc
  // but flakiness required a while loop in
  // FullscreenControllerTest::ToggleTabFullscreen. This test verifies that
  // when running serially there is no flakiness.

  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple.html");
  AddTabAtIndex(0, url, PAGE_TRANSITION_TYPED);

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(true));
  ASSERT_TRUE(IsWindowFullscreenForTabOrPending());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreenNoRetries(false));
  ASSERT_FALSE(IsWindowFullscreenForTabOrPending());
}

INSTANTIATE_TEST_CASE_P(
    ParamaterizedFullscreenControllerInteractiveTestInstance,
    ParamaterizedFullscreenControllerInteractiveTest,
    ::testing::Range<int>(TEST_TYPE_START, TEST_TYPE_END));

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen_controller_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::WebContents;

namespace {

const FilePath::CharType* kSimpleFile = FILE_PATH_LITERAL("simple.html");

}  // namespace

class FullscreenControllerBrowserTest: public FullscreenControllerTest {
 protected:
  void TestFullscreenMouseLockContentSettings();
};

#if defined(OS_MACOSX)
// http://crbug.com/104265
#define MAYBE_TestNewTabExitsFullscreen DISABLED_TestNewTabExitsFullscreen
#else
#define MAYBE_TestNewTabExitsFullscreen TestNewTabExitsFullscreen
#endif

// Tests that while in fullscreen creating a new tab will exit fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MAYBE_TestNewTabExitsFullscreen) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndexAndWait(
      0, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);

  WebContents* fullscreen_tab = browser()->GetActiveWebContents();

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));

  {
    FullscreenNotificationObserver fullscreen_observer;
    AddTabAtIndexAndWait(
        1, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

#if defined(OS_MACOSX)
// http://crbug.com/100467
#define MAYBE_TestTabExitsItselfFromFullscreen \
        FAILS_TestTabExitsItselfFromFullscreen
#else
#define MAYBE_TestTabExitsItselfFromFullscreen TestTabExitsItselfFromFullscreen
#endif

// Tests a tab exiting fullscreen will bring the browser out of fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MAYBE_TestTabExitsItselfFromFullscreen) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndexAndWait(
      0, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);

  WebContents* fullscreen_tab = browser()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, false));
}

// Tests entering fullscreen and then requesting mouse lock results in
// buttons for the user, and that after confirming the buttons are dismissed.
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       TestFullscreenBubbleMouseLockState) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndexAndWait(0, GURL(chrome::kAboutBlankURL),
                content::PAGE_TRANSITION_TYPED);
  AddTabAtIndexAndWait(1, GURL(chrome::kAboutBlankURL),
                content::PAGE_TRANSITION_TYPED);

  WebContents* fullscreen_tab = browser()->GetActiveWebContents();

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));

  // Request mouse lock and verify the bubble is waiting for user confirmation.
  RequestToLockMouse(fullscreen_tab, true, false);
  ASSERT_TRUE(IsMouseLockPermissionRequested());

  // Accept mouse lock and verify bubble no longer shows confirmation buttons.
  AcceptCurrentFullscreenOrMouseLockRequest();
  ASSERT_FALSE(IsFullscreenBubbleDisplayingButtons());
}

// Helper method to be called by multiple tests.
// Tests Fullscreen and Mouse Lock with varying content settings ALLOW & BLOCK.
void FullscreenControllerBrowserTest::TestFullscreenMouseLockContentSettings() {
  GURL url = test_server()->GetURL("simple.html");
  AddTabAtIndexAndWait(0, url, content::PAGE_TRANSITION_TYPED);
  WebContents* tab = browser()->GetActiveWebContents();

  // Validate that going fullscreen for a URL defaults to asking permision.
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(tab, true));
  ASSERT_TRUE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(tab, false));

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
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(tab, true));
  ASSERT_FALSE(IsFullscreenPermissionRequested());

  // Leaving tab in fullscreen, now test mouse lock ALLOW:

  // Validate that mouse lock defaults to asking permision.
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  RequestToLockMouse(tab, true, false);
  ASSERT_TRUE(IsMouseLockPermissionRequested());
  LostMouseLock();

  // Add content setting to ALLOW mouse lock.
  settings_map->SetContentSetting(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string(),
      CONTENT_SETTING_ALLOW);

  // Now, mouse lock should not prompt for permission.
  ASSERT_FALSE(IsMouseLockPermissionRequested());
  RequestToLockMouse(tab, true, false);
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
  RequestToLockMouse(tab, true, false);
  ASSERT_FALSE(IsMouseLockPermissionRequested());
}

// Tests fullscreen and Mouse Lock with varying content settings ALLOW & BLOCK.
IN_PROC_BROWSER_TEST_F(FullscreenControllerBrowserTest,
                       FullscreenMouseLockContentSettings) {
  TestFullscreenMouseLockContentSettings();
}

// Tests fullscreen and Mouse Lock with varying content settings ALLOW & BLOCK,
// but with the browser initiated in fullscreen mode first.
IN_PROC_BROWSER_TEST_F(FullscreenControllerBrowserTest,
                       BrowserFullscreenMouseLockContentSettings) {
  // Enter browser fullscreen first.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));
  TestFullscreenMouseLockContentSettings();
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(false));
}

// Tests Fullscreen entered in Browser, then Tab mode, then exited via Browser.
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, BrowserFullscreenExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter tab fullscreen.
  AddTabAtIndexAndWait(0, GURL(chrome::kAboutBlankURL),
                content::PAGE_TRANSITION_TYPED);
  WebContents* fullscreen_tab = browser()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));

  // Exit browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(false));
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

// Tests Browser Fullscreen remains active after Tab mode entered and exited.
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       BrowserFullscreenAfterTabFSExit) {
  // Enter browser fullscreen.
  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));

  // Enter and then exit tab fullscreen.
  AddTabAtIndexAndWait(0, GURL(chrome::kAboutBlankURL),
                content::PAGE_TRANSITION_TYPED);
  WebContents* fullscreen_tab = browser()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, false));

  // Verify browser fullscreen still active.
  ASSERT_TRUE(IsFullscreenForBrowser());
}

// Tests fullscreen entered without permision prompt for file:// urls.
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, FullscreenFileURL) {
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kSimpleFile)));
  WebContents* tab = browser()->GetActiveWebContents();

  // Validate that going fullscreen for a file does not ask permision.
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(tab, true));
  ASSERT_FALSE(IsFullscreenPermissionRequested());
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(tab, false));
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       TestTabExitsFullscreenOnNavigation) {
  ASSERT_TRUE(test_server()->Start());

  WebContents* fullscreen_tab = browser()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       TestTabExitsFullscreenOnGoBack) {
  ASSERT_TRUE(test_server()->Start());

  WebContents* fullscreen_tab = browser()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));

  GoBack();

  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       TestTabDoesntExitFullscreenOnSubFrameNavigation) {
  ASSERT_TRUE(test_server()->Start());

  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kSimpleFile)));
  GURL url_with_fragment(url.spec() + "#fragment");
  WebContents* fullscreen_tab = browser()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));
  ui_test_utils::NavigateToURL(browser(), url_with_fragment);
  ASSERT_TRUE(IsFullscreenForTabOrPending());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       TestFullscreenFromTabWhenAlreadyInBrowserFullscreenWorks) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  WebContents* fullscreen_tab = browser()->GetActiveWebContents();

  ASSERT_NO_FATAL_FAILURE(ToggleBrowserFullscreen(true));
  ASSERT_NO_FATAL_FAILURE(ToggleTabFullscreen(fullscreen_tab, true));

  GoBack();

  ASSERT_TRUE(IsFullscreenForBrowser());
  ASSERT_FALSE(IsFullscreenForTabOrPending());
}

#if defined(OS_MACOSX)
// http://crbug.com/100467
IN_PROC_BROWSER_TEST_F(
    FullscreenControllerTest, FAILS_TabEntersPresentationModeFromWindowed) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndexAndWait(
      0, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);

  WebContents* fullscreen_tab = browser()->GetActiveWebContents();

  {
    FullscreenNotificationObserver fullscreen_observer;
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    EXPECT_FALSE(browser()->window()->InPresentationMode());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, true);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
    ASSERT_TRUE(browser()->window()->InPresentationMode());
  }

  {
    FullscreenNotificationObserver fullscreen_observer;
    browser()->TogglePresentationMode();
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
    ASSERT_FALSE(browser()->window()->InPresentationMode());
  }

  if (base::mac::IsOSLionOrLater()) {
    // Test that tab fullscreen mode doesn't make presentation mode the default
    // on Lion.
    FullscreenNotificationObserver fullscreen_observer;
    browser()->ToggleFullscreenMode();
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
    ASSERT_FALSE(browser()->window()->InPresentationMode());
  }
}
#endif


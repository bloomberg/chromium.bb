// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/base/mock_host_resolver.h"

// Possible race in ChromeURLDataManager. http://crbug.com/59198
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_TabOnRemoved DISABLED_TabOnRemoved
#else
#define MAYBE_TabOnRemoved TabOnRemoved
#endif

// Window resizes are not completed by the time the callback happens,
// so these tests fail on linux/gtk. http://crbug.com/72369
#if defined(OS_LINUX) && !defined(USE_AURA)
#define MAYBE_FocusWindowDoesNotExitFullscreen \
  DISABLED_FocusWindowDoesNotExitFullscreen
#define MAYBE_UpdateWindowSizeExitsFullscreen \
  DISABLED_UpdateWindowSizeExitsFullscreen
#define MAYBE_UpdateWindowResize DISABLED_UpdateWindowResize
#define MAYBE_UpdateWindowShowState DISABLED_UpdateWindowShowState
#else

#if defined(USE_AURA) || defined(OS_MACOSX)
// Maximizing/fullscreen popup window doesn't work on aura's managed mode.
// See bug crbug.com/116305.
// Mac: http://crbug.com/103912
#define MAYBE_UpdateWindowShowState DISABLED_UpdateWindowShowState
#else
#define MAYBE_UpdateWindowShowState UpdateWindowShowState
#endif  // defined(USE_AURA) || defined(OS_MACOSX)

// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
#define MAYBE_FocusWindowDoesNotExitFullscreen \
  DISABLED_FocusWindowDoesNotExitFullscreen
#else
#define MAYBE_FocusWindowDoesNotExitFullscreen FocusWindowDoesNotExitFullscreen
#endif

#define MAYBE_UpdateWindowSizeExitsFullscreen UpdateWindowSizeExitsFullscreen
#define MAYBE_UpdateWindowResize UpdateWindowResize
#endif  // defined(OS_LINUX) && !defined(USE_AURA)

// http://crbug.com/145639
#if defined(OS_CHROMEOS)
#define MAYBE_TabEvents DISABLED_TabEvents
#else
#define MAYBE_TabEvents TabEvents
#endif  // defined(OS_CHROMEOS)

class ExtensionApiNewTabTest : public ExtensionApiTest {
 public:
  ExtensionApiNewTabTest() {}
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Override the default which InProcessBrowserTest adds if it doesn't see a
    // homepage.
    command_line->AppendSwitchASCII(
        switches::kHomePage, chrome::kChromeUINewTabURL);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiNewTabTest, Tabs) {
  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Tabs2) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud2.html")) << message_;
}

// crbug.com/149924
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabDuplicate) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "duplicate.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabUpdate) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "update.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabPinned) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "pinned.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabMove) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "move.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabEvents) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabRelativeURLs) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "relative_urls.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabQuery) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "query.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabHighlight) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "highlight.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabCrashBrowser) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crash.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabOpener) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "opener.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabGetCurrent) {
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

// Flaky on the trybots. See http://crbug.com/96725.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabConnect) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabOnRemoved) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabReload) {
  ASSERT_TRUE(RunExtensionTest("tabs/reload")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_CaptureVisibleTabJpeg) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_jpeg.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_CaptureVisibleTabPng) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_png.html")) << message_;
}

// Times out on non-Windows.
// See http://crbug.com/80212
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_CaptureVisibleTabRace) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_race.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleFile) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_file.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleNoFile) {
  ASSERT_TRUE(RunExtensionSubtest(
      "tabs/capture_visible_tab", "test_nofile.html",
      ExtensionApiTest::kFlagNone)) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots,
                                               true);
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_disabled.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsOnUpdated) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_updated")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_FocusWindowDoesNotExitFullscreen) {
  browser()->window()->EnterFullscreen(
      GURL(), FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
  bool is_fullscreen = browser()->window()->IsFullscreen();
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_EQ(is_fullscreen, browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_UpdateWindowSizeExitsFullscreen) {
  browser()->window()->EnterFullscreen(
      GURL(), FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
  ASSERT_TRUE(RunExtensionTest("window_update/sizing")) << message_;
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_UpdateWindowResize) {
  ASSERT_TRUE(RunExtensionTest("window_update/resize")) << message_;
}

#if defined(OS_WIN) && !defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FocusWindowDoesNotUnmaximize) {
  gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  ::SendMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_TRUE(::IsZoomed(window));
}
#endif  // OS_WIN

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_UpdateWindowShowState) {
  ASSERT_TRUE(RunExtensionTest("window_update/show_state")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoDisabledByPref) {
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);

  // This makes sure that creating an incognito window fails due to pref
  // (policy) being set.
  ASSERT_TRUE(RunExtensionTest("tabs/incognito_disabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_GetViewsOfCreatedPopup) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_popup.html"))
      << message_;
}
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_GetViewsOfCreatedWindow) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_window.html"))
      << message_;
}

// Adding a new test? Awesome. But API tests are the old hotness. The
// new hotness is extension_test_utils. See tabs_test.cc for an example.
// We are trying to phase out many uses of API tests as they tend to be flaky.

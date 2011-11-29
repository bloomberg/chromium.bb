// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "net/base/mock_host_resolver.h"

// Possible race in ChromeURLDataManager. http://crbug.com/59198
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_TabOnRemoved DISABLED_TabOnRemoved
#else
#define MAYBE_TabOnRemoved TabOnRemoved
#endif

// Window resizes are not completed by the time the callback happens,
// so these tests fail on linux. http://crbug.com/72369
#if defined(OS_LINUX)
#define MAYBE_FocusWindowDoesNotExitFullscreen \
  DISABLED_FocusWindowDoesNotExitFullscreen
#define MAYBE_UpdateWindowSizeExitsFullscreen \
  DISABLED_UpdateWindowSizeExitsFullscreen
#define MAYBE_UpdateWindowShowState \
  DISABLED_UpdateWindowShowState
#else
#define MAYBE_FocusWindowDoesNotExitFullscreen FocusWindowDoesNotExitFullscreen
#define MAYBE_UpdateWindowSizeExitsFullscreen UpdateWindowSizeExitsFullscreen

// http://crbug.com/105356 , this test is failing browser_tests on Windows.
#if defined(OS_WIN)
#define MAYBE_UpdateWindowShowState FAILS_UpdateWindowShowState
#else
#define MAYBE_UpdateWindowShowState UpdateWindowShowState
#endif

#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Tabs) {
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

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabUpdate) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "update.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabPinned) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "pinned.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabMove) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "move.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabEvents) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabRelativeURLs) {
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

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabGetCurrent) {
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

// Flaky on the trybots. See http://crbug.com/96725.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FLAKY_TabConnect) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabOnRemoved) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabReload) {
  ASSERT_TRUE(RunExtensionTest("tabs/reload")) << message_;
}

// Test is timing out on linux and cros and flaky on others.
// See http://crbug.com/83876
#if defined(OS_LINUX)
#define MAYBE_CaptureVisibleTabJpeg DISABLED_CaptureVisibleTabJpeg
#else
#define MAYBE_CaptureVisibleTabJpeg FLAKY_CaptureVisibleTabJpeg
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_CaptureVisibleTabJpeg) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_jpeg.html")) << message_;
}

// Test is timing out on linux and cros and flaky on others.
// See http://crbug.com/83876
#if defined(OS_LINUX)
#define MAYBE_CaptureVisibleTabPng DISABLED_CaptureVisibleTabPng
#else
#define MAYBE_CaptureVisibleTabPng FLAKY_CaptureVisibleTabPng
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_CaptureVisibleTabPng) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_png.html")) << message_;
}

// Times out on non-Windows. See http://crbug.com/80212
#if defined(OS_WIN)
#define MAYBE_CaptureVisibleTabRace DISABLED_CaptureVisibleTabRace
#else
#define MAYBE_CaptureVisibleTabRace DISABLED_CaptureVisibleTabRace
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_CaptureVisibleTabRace) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_race.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleFile) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_file.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleNoFile) {
  ASSERT_TRUE(RunExtensionSubtestNoFileAccess("tabs/capture_visible_tab",
                                              "test_nofile.html")) << message_;
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

#if defined(OS_WIN) && !defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FocusWindowDoesNotUnmaximize) {
  gfx::NativeWindow window = browser()->window()->GetNativeHandle();
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

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetViewsOfCreatedPopup) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_popup.html"))
      << message_;
}
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetViewsOfCreatedWindow) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_window.html"))
      << message_;
}

// Adding a new test? Awesome. But API tests are the old hotness. The
// new hotness is extension_test_utils. See extension_tabs_test.cc for
// an example. We are trying to phase out many uses of API tests as
// they tend to be flaky.

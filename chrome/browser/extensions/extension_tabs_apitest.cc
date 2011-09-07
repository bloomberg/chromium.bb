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
#else
#define MAYBE_FocusWindowDoesNotExitFullscreen FocusWindowDoesNotExitFullscreen
#define MAYBE_UpdateWindowSizeExitsFullscreen UpdateWindowSizeExitsFullscreen
#endif

// In the touch build, this fails reliably. http://crbug.com/85191
#if defined(TOUCH_UI)
#define MAYBE_GetViewsOfCreatedWindow DISABLED_GetViewsOfCreatedWindow
#else
#define MAYBE_GetViewsOfCreatedWindow GetViewsOfCreatedWindow
#endif

// In the touch build, this fails unreliably. http://crbug.com/85226
#if defined(TOUCH_UI)
#define MAYBE_GetViewsOfCreatedPopup FLAKY_GetViewsOfCreatedPopup
#else
#define MAYBE_GetViewsOfCreatedPopup GetViewsOfCreatedPopup
#endif

// In the touch build, this fails reliably. http://crbug.com/85190
#if defined(TOUCH_UI)
#define MAYBE_TabEvents DISABLED_TabEvents
#else
#define MAYBE_TabEvents TabEvents
#endif

// In the touch build, this fails unreliably. http://crbug.com/85193
#if defined(TOUCH_UI)
#define MAYBE_TabMove FLAKY_TabMove
#else
#define MAYBE_TabMove TabMove
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Tabs) {
  ASSERT_TRUE(StartTestServer());

  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Tabs2) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud2.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabPinned) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "pinned.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabMove) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "move.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabEvents) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabRelativeURLs) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "relative_urls.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabCrashBrowser) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crash.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabGetCurrent) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabConnect) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabOnRemoved) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabReload) {
  ASSERT_TRUE(StartTestServer());
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
  ASSERT_TRUE(StartTestServer());
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
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/on_updated")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_FocusWindowDoesNotExitFullscreen) {
  browser()->window()->SetFullscreen(true);
  bool is_fullscreen = browser()->window()->IsFullscreen();
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_EQ(is_fullscreen, browser()->window()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_UpdateWindowSizeExitsFullscreen) {
  browser()->window()->SetFullscreen(true);
  ASSERT_TRUE(RunExtensionTest("window_update/sizing")) << message_;
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FocusWindowDoesNotUnmaximize) {
  gfx::NativeWindow window = browser()->window()->GetNativeHandle();
  ::SendMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_TRUE(::IsZoomed(window));
}
#endif  // OS_WIN

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoDisabledByPref) {
  ASSERT_TRUE(StartTestServer());

  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);

  // This makes sure that creating an incognito window fails due to pref
  // (policy) being set.
  ASSERT_TRUE(RunExtensionTest("tabs/incognito_disabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_GetViewsOfCreatedPopup) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_popup.html"))
      << message_;
}
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_GetViewsOfCreatedWindow) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_window.html"))
      << message_;
}

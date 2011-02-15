// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/browser_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "net/base/mock_host_resolver.h"

// Possible race in ChromeURLDataManager. http://crbug.com/59198
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_TabOnRemoved DISABLED_TabOnRemoved
#else
#define MAYBE_TabOnRemoved TabOnRemoved
#endif

// Crashes on linux views. http://crbug.com/61592
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
#define MAYBE_Tabs DISABLED_Tabs
#else
#define MAYBE_Tabs Tabs
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

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Tabs) {
  ASSERT_TRUE(StartTestServer());

  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabPinned) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "pinned.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabMove) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "move.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabEvents) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabRelativeURLs) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "relative_urls.html"))
      << message_;
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

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleTabJpeg) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_jpeg.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleTabPng) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_png.html")) << message_;
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

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kIncognitoEnabled, false);

  // This makes sure that creating an incognito window fails due to pref
  // (policy) being set.
  ASSERT_TRUE(RunExtensionTest("tabs/incognito_disabled")) << message_;
}

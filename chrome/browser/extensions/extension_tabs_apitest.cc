// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"

#if defined(OS_WIN)
// This test times out on win.
// http://crbug.com/58269
#define MAYBE_Tabs FAILS_Tabs
#else
#define MAYBE_Tabs Tabs
#endif

// Possible race in ChromeURLDataManager. http://crbug.com/59198
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_TabOnRemoved FLAKY_TabOnRemoved
#else
#define MAYBE_TabOnRemoved TabOnRemoved
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Tabs) {
  ASSERT_TRUE(test_server()->Start());

  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionTest("tabs/basics")) << message_;
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
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_jpeg.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleTabPng) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_png.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsOnUpdated) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/on_updated")) << message_;
}

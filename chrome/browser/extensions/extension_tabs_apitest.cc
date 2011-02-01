// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"

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

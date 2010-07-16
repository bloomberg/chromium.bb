// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"

// Tabs is flaky on chromeos and linux views debug build.
// http://crbug.com/48920
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS) && !defined(NDEBUG)
#define MAYBE_Tabs FLAKY_Tabs
#else
#define MAYBE_Tabs Tabs
#endif

// TabOnRemoved is flaky on chromeos and linux views debug build.
// http://crbug.com/49258
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS) && !defined(NDEBUG)
#define MAYBE_TabOnRemoved FLAKY_TabOnRemoved
#else
#define MAYBE_TabOnRemoved TabOnRemoved
#endif

// CaptureVisibleTab fails on karmic 64 bit.
// http://crbug.com/49040
#if defined(OS_LINUX) && defined(__x86_64__) && !defined(NDEBUG)
#define MAYBE_CaptureVisibleTab FLAKY_CaptureVisibleTab
#else
#define MAYBE_CaptureVisibleTab CaptureVisibleTab
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Tabs) {
  ASSERT_TRUE(StartHTTPServer());

  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionTest("tabs/basics")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabGetCurrent) {
  ASSERT_TRUE(StartHTTPServer());
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabConnect) {
  ASSERT_TRUE(StartHTTPServer());
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabOnRemoved) {
  ASSERT_TRUE(StartHTTPServer());
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_CaptureVisibleTab) {
  ASSERT_TRUE(StartHTTPServer());
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsOnUpdated) {
  ASSERT_TRUE(StartHTTPServer());

  ASSERT_TRUE(RunExtensionTest("tabs/on_updated")) << message_;
}

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"

// Disabled due to timeouts, see http://crbug.com/39843, http://crbug.com/43440.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Tabs) {
  ASSERT_TRUE(StartHTTPServer());

  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionTest("tabs/basics")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Tabs2) {
  ASSERT_TRUE(StartHTTPServer());

  // This test runs through additional tabs functionality.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionTest("tabs/basics2")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, CaptureVisibleTab) {
  ASSERT_TRUE(StartHTTPServer());

  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab")) << message_;
}

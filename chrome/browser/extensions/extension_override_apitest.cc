// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, OverrideNewtab) {
  ASSERT_TRUE(RunExtensionTest("override/newtab")) << message_;
  {
    ResultCatcher catcher;
    // Navigate to the new tab page.  The overridden new tab page
    // will call chrome.test.notifyPass() .
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // TODO(erikkay) Load a second extension with the same override.
  // Verify behavior, then unload the first and verify behavior, etc.
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, OverrideHistory) {
  ASSERT_TRUE(RunExtensionTest("override/history")) << message_;
  {
    ResultCatcher catcher;
    // Navigate to the history page.  The overridden history page
    // will call chrome.test.notifyPass() .
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://history/"));
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

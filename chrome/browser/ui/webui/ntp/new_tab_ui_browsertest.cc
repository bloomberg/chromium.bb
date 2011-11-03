// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

class NewTabUIBrowserTest : public InProcessBrowserTest {
 public:
  NewTabUIBrowserTest() {
    EnableDOMAutomation();
  }
};

// Ensure that chrome-internal: still loads the NTP.
// See http://crbug.com/6564.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, ChromeInternalLoadsNTP) {
  // Go to the "new tab page" using its old url, rather than chrome://newtab.
  // Ensure that we get there by checking for non-empty page content.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome-internal:"));
  bool empty_inner_html = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetTabContentsAt(0)->render_view_host(), L"",
      L"window.domAutomationController.send(document.body.innerHTML == '')",
      &empty_inner_html));
  ASSERT_FALSE(empty_inner_html);
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "base/test/thread_test_helper.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_switches.h"

class MagicIframeBrowserTest : public InProcessBrowserTest {
 public:
  MagicIframeBrowserTest() {
    EnableDOMAutomation();
  }

  GURL GetTestURL(const std::string& path);
};

GURL MagicIframeBrowserTest::GetTestURL(const std::string& path) {
  std::string url_path = "files/magic_iframe/";
  url_path.append(path);
  return test_server()->GetURL(url_path);
}

// Verifies that reparenting of iframe between different pages works as
// expected, including smooth transfer of resources that are still being loaded.
// The test failure manifests as the ongoing XHR request is not finishing
// (silently aborted) when the test iframe is transferred between pages (and
// the original page closes). This causes the final navigation to not complete
// and test terminated due to a timeout.

// Currently disabled (http://crbug.com/55200, work in progress).
IN_PROC_BROWSER_TEST_F(MagicIframeBrowserTest,
                       DISABLED_TransferIframeCloseWindow) {
  ASSERT_TRUE(test_server()->Start());
  GURL url(GetTestURL("iframe-reparenting-close-window.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 3);
  std::string result;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(getLog())", &result));
  ASSERT_NE(result.find("DONE"), std::string::npos) << result;
}


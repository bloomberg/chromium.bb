// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

using content::WebContents;

namespace {

const char kSharedWorkerTestPage[] =
    "files/workers/workers_ui_shared_worker.html";
const char kSharedWorkerJs[] =
    "files/workers/workers_ui_shared_worker.js";

class InspectUITest : public InProcessBrowserTest {
 public:
  InspectUITest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InspectUITest);
};

// The test fails on Mac OS X and Windows, see crbug.com/89583
// Intermittently fails on Linux.
IN_PROC_BROWSER_TEST_F(InspectUITest, DISABLED_SharedWorkersList) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kSharedWorkerTestPage);
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUIInspectURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  WebContents* web_contents = chrome::GetActiveWebContents(browser());
  ASSERT_TRUE(web_contents != NULL);

  std::string result;
  ASSERT_TRUE(
      content::ExecuteJavaScriptAndExtractString(
          web_contents->GetRenderViewHost(),
          "",
          "window.domAutomationController.send("
          "    '' + document.body.textContent);",
          &result));
  ASSERT_TRUE(result.find(kSharedWorkerJs) != std::string::npos);
  ASSERT_TRUE(result.find(kSharedWorkerTestPage) != std::string::npos);
}

}  // namespace

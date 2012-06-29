// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "net/base/net_util.h"
#include "webkit/dom_storage/dom_storage_types.h"

// This browser test is aimed towards exercising the DomStorage system
// from end-to-end.
class DomStorageBrowserTest : public InProcessBrowserTest {
 public:
  DomStorageBrowserTest() {
    EnableDOMAutomation();
  }

  GURL GetTestURL(const char* name) {
    FilePath file_name = FilePath().AppendASCII(name);
    FilePath dir;
    PathService::Get(content::DIR_TEST_DATA, &dir);
    return net::FilePathToFileURL(
        dir.Append(FILE_PATH_LITERAL("dom_storage")).Append(file_name));
  }

  void SimpleTest(const GURL& test_url, bool incognito) {
    // The test page will perform tests then navigate to either
    // a #pass or #fail ref.
    Browser* the_browser = incognito ? CreateIncognitoBrowser() : browser();
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        the_browser, test_url, 2);
    std::string result =
        chrome::GetActiveWebContents(the_browser)->GetURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          chrome::GetActiveWebContents(the_browser)->GetRenderViewHost(), L"",
          L"window.domAutomationController.send(getLog())", &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }
};

static const bool kIncognito = true;
static const bool kNotIncognito = false;

IN_PROC_BROWSER_TEST_F(DomStorageBrowserTest, SanityCheck) {
  SimpleTest(GetTestURL("sanity_check.html"), kNotIncognito);
}

IN_PROC_BROWSER_TEST_F(DomStorageBrowserTest, SanityCheckIncognito) {
  SimpleTest(GetTestURL("sanity_check.html"), kIncognito);
}

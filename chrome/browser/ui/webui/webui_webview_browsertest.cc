// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class WebUIWebViewBrowserTest : public WebUIBrowserTest {
 public:
  WebUIWebViewBrowserTest() {}

  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(
        base::FilePath(FILE_PATH_LITERAL("webview_execute_script_test.js")));

    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  GURL GetTestUrl(const std::string& path) const {
    return embedded_test_server()->base_url().Resolve(path);
  }

  GURL GetWebViewEnabledWebUIURL() const {
    return GURL(chrome::kChromeUIChromeSigninURL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUIWebViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, ExecuteScriptCode) {
  ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testExecuteScriptCode",
      new base::StringValue(GetTestUrl("empty.html").spec())));
}

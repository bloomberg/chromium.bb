// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"

class CalculatorBrowserTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(CalculatorBrowserTest, Model) {
  FilePath test_file;
  PathService::Get(chrome::DIR_TEST_DATA, &test_file);
  test_file = test_file.DirName().DirName()
      .AppendASCII("common").AppendASCII("extensions").AppendASCII("docs")
      .AppendASCII("examples").AppendASCII("apps").AppendASCII("calculator")
      .AppendASCII("tests").AppendASCII("automatic.html");

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

  bool success;
  bool executed = content::ExecuteJavaScriptAndExtractBool(
      chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(window.runTests().success)",
      &success);

  ASSERT_TRUE(executed);
  ASSERT_TRUE(success);
}

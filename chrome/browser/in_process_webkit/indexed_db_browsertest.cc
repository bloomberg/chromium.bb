// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

// This browser test is aimed towards exercising the IndexedDB bindings and
// the actual implementation that lives in the browser side (in_process_webkit).
class IndexedDBBrowserTest : public InProcessBrowserTest {
 public:
  IndexedDBBrowserTest() {
    EnableDOMAutomation();
  }

  // From InProcessBrowserTest.
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableIndexedDatabase);
  }

  GURL testUrl(const FilePath& file_path) {
    const FilePath kTestDir(FILE_PATH_LITERAL("indexeddb"));
    return ui_test_utils::GetTestUrl(kTestDir, file_path);
  }

  std::string GetTestLog() {
    std::string script = "window.domAutomationController.send(getLog())";
    std::string js_result;
    ui_test_utils::ExecuteJavaScriptAndExtractString(
        browser()->GetSelectedTabContents()->render_view_host(),
        L"", UTF8ToWide(script), &js_result);
    return js_result;
  }

  void SimpleTest(const GURL& test_url) {
    // The test page will open a cursor on IndexedDB, then navigate to either a
    // #pass or #fail ref.
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), test_url, 2);
    std::string result = browser()->GetSelectedTabContents()->GetURL().ref();
    if (result != "pass") {
      std::string js_result = GetTestLog();
      FAIL() << "Failed: " << js_result;
    }
  }
};

// Fails after WebKit roll 67957:68016, http://crbug.com/56509
#if defined(OS_LINUX)
#define MAYBE_CursorTest FAILS_CursorTest
#define MAYBE_IndexTest FAILS_IndexTest
#define MAYBE_KeyPathTest FAILS_KeyPathTest
#else
#define MAYBE_CursorTest CursorTest
#define MAYBE_IndexTest IndexTest
#define MAYBE_KeyPathTest KeyPathTest
#endif

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, MAYBE_CursorTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("cursor_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, MAYBE_IndexTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("index_test.html"))));
}

IN_PROC_BROWSER_TEST_F(IndexedDBBrowserTest, MAYBE_KeyPathTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("key_path_test.html"))));
}

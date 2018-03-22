// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"

namespace file_manager {

// Test fixture class for FileManager UI.
class FileManagerUITest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // --disable-web-security required to load resources from
    // files and from chrome://resources/... urls.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

  void RunTest() {
    base::FilePath root_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &root_path));

    // Load test.html.
    const GURL url = net::FilePathToFileURL(root_path.Append(
        FILE_PATH_LITERAL("ui/file_manager/file_manager/test.html")));
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    ui_test_utils::NavigateToURL(browser(), url);

    // Load and run specified test file.
    content::DOMMessageQueue message_queue;
    ExecuteScriptAsync(web_contents, "runTests()");

    // Wait for JS to call domAutomationController.send("SUCCESS").
    std::string message;
    do {
      EXPECT_TRUE(message_queue.WaitForMessage(&message));
    } while (message == "\"PENDING\"");

    EXPECT_TRUE(message == "\"SUCCESS\"");
  }
};

IN_PROC_BROWSER_TEST_F(FileManagerUITest, UI) {
  RunTest();
}

}  // namespace file_manager

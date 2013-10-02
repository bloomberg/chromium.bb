/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/keyboard/keyboard_switches.h"

namespace {

const base::FilePath kWebuiTestDir =
    base::FilePath(FILE_PATH_LITERAL("webui"));

const base::FilePath kVirtualKeyboardTestDir =
    base::FilePath(FILE_PATH_LITERAL("chromeos/virtual_keyboard"));

const base::FilePath kMockController =
    base::FilePath(FILE_PATH_LITERAL("mock_controller.js"));

const base::FilePath kMockTimer =
    base::FilePath(FILE_PATH_LITERAL("mock_timer.js"));

const base::FilePath kBaseKeyboardTestFramework =
    base::FilePath(FILE_PATH_LITERAL("virtual_keyboard_test_base.js"));

}  // namespace

class VirtualKeyboardBrowserTest : public InProcessBrowserTest {
 public:

  /**
   * Ensure that the virtual keyboard is enabled.
   */
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
  }

  /**
   * Injects javascript in |file| into the keyboard page and runs test methods.
   */
  void RunTest(const base::FilePath& file) {
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://keyboard"));

    content::RenderViewHost* rvh = browser()->tab_strip_model()
        ->GetActiveWebContents()->GetRenderViewHost();
    ASSERT_TRUE(rvh);

    // Inject testing scripts.
    InjectJavascript(kWebuiTestDir, kMockController);
    InjectJavascript(kWebuiTestDir, kMockTimer);
    InjectJavascript(kVirtualKeyboardTestDir, kBaseKeyboardTestFramework);
    InjectJavascript(kVirtualKeyboardTestDir, file);

    ASSERT_TRUE(content::ExecuteScript(rvh, utf8_content_));

    // Inject DOM-automation test harness and run tests.
    std::vector<int> resource_ids;
    EXPECT_TRUE(ExecuteWebUIResourceTest(rvh, resource_ids));
  }

 private:

  /**
   * Injects javascript into the keyboard page.  The test |file| is in
   * directory |dir| relative to the root testing directory.
   */
  void InjectJavascript(const base::FilePath& dir,
                        const base::FilePath& file) {
    base::FilePath path = ui_test_utils::GetTestFilePath(dir, file);
    std::string library_content;
    ASSERT_TRUE(base::ReadFileToString(path, &library_content))
        << path.value();
    utf8_content_.append(library_content);
    utf8_content_.append(";\n");
  }

  std::string utf8_content_;
};

IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, TypingTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("typing_test.js")));
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, ControlKeysTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("control_keys_test.js")));
}
// TODO(kevers|rsadam|bshe):  Add UI tests for remaining virtual keyboard
// functionality.

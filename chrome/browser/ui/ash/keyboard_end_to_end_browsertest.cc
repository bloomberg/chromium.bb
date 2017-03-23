// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/keyboard/content/keyboard_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_test_util.h"

namespace {

bool IsKeyboardVisible() {
  return keyboard::KeyboardController::GetInstance()->keyboard_visible();
}

// Simulates a click on the middle of the DOM element with the given |id|.
void ClickElementWithId(content::WebContents* web_contents,
                        const std::string& id) {
  int x;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "var bounds = document.getElementById('" + id +
          "').getBoundingClientRect();"
          "domAutomationController.send("
          "    Math.floor(bounds.left + bounds.width / 2));",
      &x));
  int y;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "var bounds = document.getElementById('" + id +
          "').getBoundingClientRect();"
          "domAutomationController.send("
          "    Math.floor(bounds.top + bounds.height / 2));",
      &y));
  content::SimulateMouseClickAt(
      web_contents, 0, blink::WebMouseEvent::Button::Left, gfx::Point(x, y));
}

}  // namespace

namespace keyboard {

class KeyboardEndToEndTest : public InProcessBrowserTest {
 public:
  KeyboardEndToEndTest() {}
  ~KeyboardEndToEndTest() override {}

  // Ensure that the virtual keyboard is enabled.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
  }
};

IN_PROC_BROWSER_TEST_F(KeyboardEndToEndTest, OpenIfFocusedOnClick) {
  GURL test_url =
      ui_test_utils::GetTestUrl(base::FilePath("chromeos/virtual_keyboard"),
                                base::FilePath("inputs.html"));
  ui_test_utils::NavigateToURL(browser(), test_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  EXPECT_FALSE(IsKeyboardVisible());

  ClickElementWithId(web_contents, "text");

  ASSERT_TRUE(keyboard::WaitUntilShown());
  EXPECT_TRUE(IsKeyboardVisible());

  ClickElementWithId(web_contents, "blur");
  ASSERT_TRUE(keyboard::WaitUntilHidden());
  EXPECT_FALSE(IsKeyboardVisible());
}

}  // namespace keyboard

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

class SwitchAccessTest : public InProcessBrowserTest {
 public:
  void SendVirtualKeyPress(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, key, false, false, false, false)));
  }

  void EnableSwitchAccess(const std::set<int>& key_codes = {'1', '2', '3',
                                                            '4'}) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kEnableExperimentalAccessibilityFeatures);

    AccessibilityManager* manager = AccessibilityManager::Get();
    manager->SetSwitchAccessEnabled(true);
    manager->SetSwitchAccessKeys(key_codes);

    EXPECT_TRUE(manager->IsSwitchAccessEnabled());
  }

 protected:
  SwitchAccessTest() = default;
  ~SwitchAccessTest() override = default;

  void SetUpOnMainThread() override {}
};

IN_PROC_BROWSER_TEST_F(SwitchAccessTest, IgnoresVirtualKeyEvents) {
  EnableSwitchAccess({'1', '2', '3', '4'});

  // Load a webpage with a text box
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,<input type=text id=in>"));

  // Put focus in the text box
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_TAB);

  // Send a virtual key event for one of the keys taken by switch access
  SendVirtualKeyPress(ui::KeyboardCode::VKEY_1);

  // Check that the text field received the keystroke
  std::string output;
  std::string script =
      "window.domAutomationController.send("
      "document.getElementById('in').value)";
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0), script, &output));
  EXPECT_STREQ(output.c_str(), "1");
}

}  // namespace chromeos

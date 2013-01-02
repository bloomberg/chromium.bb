// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/win/metro.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/content_browser_test.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/win/mock_tsf_bridge.h"
#include "ui/base/ime/win/tsf_bridge.h"

namespace content {
class RenderWidgetHostViewWinTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewWinTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableTextServicesFramework);
  }
};

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTest,
                       DISABLED_SwichToPasswordField) {
  ui::MockTSFBridge mock_bridge;
  ui::TSFBridge* old_bridge = ui::TSFBridge::ReplaceForTesting(&mock_bridge);
  GURL test_url = GetTestUrl("textinput", "ime_enable_disable_test.html");

  NavigateToURL(shell(), test_url);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, mock_bridge.latest_text_iput_type());

  // Focus to the text field, the IME should be enabled.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(text01_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  // Focus to the password field, the IME should be disabled.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(password02_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  ui::TSFBridge::ReplaceForTesting(old_bridge);
}

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTest,
                       DISABLED_SwitchToSameField) {
  ui::MockTSFBridge mock_bridge;
  ui::TSFBridge* old_bridge = ui::TSFBridge::ReplaceForTesting(&mock_bridge);
  GURL test_url = GetTestUrl("textinput", "ime_enable_disable_test.html");

  NavigateToURL(shell(), test_url);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, mock_bridge.latest_text_iput_type());

  // Focus to the text field, the IME should be enabled.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(text01_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  // Focus to another text field, the IME should be enabled.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(text02_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  ui::TSFBridge::ReplaceForTesting(old_bridge);
}

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTest,
                       DISABLED_SwitchToSamePasswordField) {
  ui::MockTSFBridge mock_bridge;
  ui::TSFBridge* old_bridge = ui::TSFBridge::ReplaceForTesting(&mock_bridge);
  GURL test_url = GetTestUrl("textinput", "ime_enable_disable_test.html");

  NavigateToURL(shell(), test_url);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, mock_bridge.latest_text_iput_type());

  // Focus to the password field, the IME should be disabled.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(password01_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  // Focus to the another password field, the IME should be disabled.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(password02_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  ui::TSFBridge::ReplaceForTesting(old_bridge);
}

}  // namespace content

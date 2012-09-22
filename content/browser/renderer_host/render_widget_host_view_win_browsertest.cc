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
#include "ui/base/win/mock_tsf_bridge.h"
#include "ui/base/win/tsf_bridge.h"

namespace {
class RenderWidgetHostViewWinTest : public content::ContentBrowserTest {
 public:
  RenderWidgetHostViewWinTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableTextServicesFramework);
  }
};

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTest,
                       DISABLED_SwichToPasswordField) {
  ui::MockTsfBridge mock_bridge;
  ui::TsfBridge* old_bridge = ui::TsfBridge::ReplaceForTesting(&mock_bridge);
  GURL test_url = content::GetTestUrl("textinput",
                                      "ime_enable_disable_test.html");

  content::NavigateToURL(shell(), test_url);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, mock_bridge.latest_text_iput_type());

  // Focus to the text field, the IME should be enabled.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(text01_focus());",
      &success));
  EXPECT_TRUE(success);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  // Focus to the password field, the IME should be disabled.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(password02_focus());",
      &success));
  EXPECT_TRUE(success);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  ui::TsfBridge::ReplaceForTesting(old_bridge);
}

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTest,
                       DISABLED_SwitchToSameField) {
  ui::MockTsfBridge mock_bridge;
  ui::TsfBridge* old_bridge = ui::TsfBridge::ReplaceForTesting(&mock_bridge);
  GURL test_url = content::GetTestUrl("textinput",
                                      "ime_enable_disable_test.html");

  content::NavigateToURL(shell(), test_url);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, mock_bridge.latest_text_iput_type());

  // Focus to the text field, the IME should be enabled.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(text01_focus());",
      &success));
  EXPECT_TRUE(success);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  // Focus to another text field, the IME should be enabled.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(text02_focus());",
      &success));
  EXPECT_TRUE(success);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  ui::TsfBridge::ReplaceForTesting(old_bridge);
}

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTest,
                       DISABLED_SwitchToSamePasswordField) {
  ui::MockTsfBridge mock_bridge;
  ui::TsfBridge* old_bridge = ui::TsfBridge::ReplaceForTesting(&mock_bridge);
  GURL test_url = content::GetTestUrl("textinput",
                                      "ime_enable_disable_test.html");

  content::NavigateToURL(shell(), test_url);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, mock_bridge.latest_text_iput_type());

  // Focus to the password field, the IME should be disabled.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(password01_focus());",
      &success));
  EXPECT_TRUE(success);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  // Focus to the another password field, the IME should be disabled.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(password02_focus());",
      &success));
  EXPECT_TRUE(success);
  content::WaitForLoadStop(shell()->web_contents());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  ui::TsfBridge::ReplaceForTesting(old_bridge);
}
}  // namespace

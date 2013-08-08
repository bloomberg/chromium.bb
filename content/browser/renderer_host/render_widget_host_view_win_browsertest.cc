// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/win/metro.h"
#include "content/browser/renderer_host/render_widget_host_view_win.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/content_browser_test.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/win/imm32_manager.h"
#include "ui/base/ime/win/mock_tsf_bridge.h"
#include "ui/base/ime/win/tsf_bridge.h"

namespace content {
namespace {

class MockIMM32Manager : public ui::IMM32Manager {
 public:
   MockIMM32Manager()
       : window_handle_(NULL),
         input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
         call_count_(0) {
   }
  virtual ~MockIMM32Manager() {}

  virtual void SetTextInputMode(HWND window_handle,
                                ui::TextInputMode input_mode) OVERRIDE {
    ++call_count_;
    window_handle_ = window_handle;
    input_mode_ = input_mode;
  }

  void Reset() {
    window_handle_ = NULL;
    input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;
    call_count_ = 0;
  }

  HWND window_handle() const { return window_handle_; }
  ui::TextInputMode input_mode() const { return input_mode_; }
  size_t call_count() const { return call_count_; }

 private:
  HWND window_handle_;
  ui::TextInputMode input_mode_;
  size_t call_count_;

  DISALLOW_COPY_AND_ASSIGN(MockIMM32Manager);
};

// Testing class serving initialized RenderWidgetHostViewWin instance;
class RenderWidgetHostViewWinBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewWinBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ContentBrowserTest::SetUpOnMainThread();

    NavigateToURL(shell(), GURL("about:blank"));

    view_ = static_cast<RenderWidgetHostViewWin*>(
        RenderWidgetHostViewPort::FromRWHV(
            shell()->web_contents()->GetRenderViewHost()->GetView()));
    CHECK(view_);
  }

 protected:
  RenderWidgetHostViewWin* view_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinBrowserTest,
                       TextInputTypeChanged) {
  ASSERT_TRUE(view_->m_hWnd);

  MockIMM32Manager* mock = new MockIMM32Manager();
  mock->Reset();
  view_->imm32_manager_.reset(mock);
  view_->TextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE, false,
                              ui::TEXT_INPUT_MODE_EMAIL);

  EXPECT_EQ(1, mock->call_count());
  EXPECT_EQ(view_->m_hWnd, mock->window_handle());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_EMAIL, mock->input_mode());

  mock->Reset();
  view_->TextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE, false,
                              ui::TEXT_INPUT_MODE_EMAIL);
  EXPECT_EQ(0, mock->call_count());
}

class RenderWidgetHostViewWinTSFTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewWinTSFTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableTextServicesFramework);
  }
};

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTSFTest,
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
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(text01_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  // Focus to the password field, the IME should be disabled.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(password02_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  ui::TSFBridge::ReplaceForTesting(old_bridge);
}

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTSFTest,
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
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(text01_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  // Focus to another text field, the IME should be enabled.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(text02_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, mock_bridge.latest_text_iput_type());

  ui::TSFBridge::ReplaceForTesting(old_bridge);
}

// crbug.com/151798
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewWinTSFTest,
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
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(password01_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  // Focus to the another password field, the IME should be disabled.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(password02_focus());",
      &success));
  EXPECT_TRUE(success);
  WaitForLoadStop(shell()->web_contents());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, mock_bridge.latest_text_iput_type());

  ui::TSFBridge::ReplaceForTesting(old_bridge);
}

}  // namespace content

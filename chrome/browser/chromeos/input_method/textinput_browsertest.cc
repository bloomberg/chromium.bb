// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/text_input_client.h"

namespace chromeos {

class TextInputTest : public InProcessBrowserTest,
                      public ui::MockInputMethod::Observer {
 public:
  TextInputTest()
      : waiting_text_input_type_change_(false),
        latest_text_input_type_(ui::TEXT_INPUT_TYPE_NONE) {}
  virtual ~TextInputTest() {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ui::SetUpInputMethodFacotryForTesting();
  }

  ui::MockInputMethod* GetInputMethod() {
    ui::MockInputMethod* input_method =
        static_cast<ui::MockInputMethod*>(
            ash::Shell::GetPrimaryRootWindow()->GetProperty(
                aura::client::kRootWindowInputMethodKey));
    CHECK(input_method);
    return input_method;
  }

  // ui::MockInputMethod::Observer override.
  void OnTextInputTypeChanged(const ui::TextInputClient* client) OVERRIDE {
    latest_text_input_type_ = client->GetTextInputType();
    if (waiting_text_input_type_change_)
      MessageLoop::current()->Quit();
  }

  // Wait until the latest text input type become |expected_type|. If the
  // already latest text input is |expected_type|, return immediately.
  void WaitForTextInputStateChanged(ui::TextInputType expected_type) {
    if (latest_text_input_type_ == expected_type)
      return;
    waiting_text_input_type_change_ = true;
    while (latest_text_input_type_ != expected_type)
      content::RunMessageLoop();
    waiting_text_input_type_change_ = false;
  }

  bool waiting_text_input_type_change_;
  ui::TextInputType latest_text_input_type_;
};

IN_PROC_BROWSER_TEST_F(TextInputTest, SwitchToPasswordFieldTest) {
  GetInputMethod()->AddObserver(this);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath("textinput"),
      base::FilePath("ime_enable_disable_test.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  bool worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(text01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, latest_text_input_type_);

  worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(password01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, latest_text_input_type_);
  GetInputMethod()->RemoveObserver(this);
}

} // namespace chromeos

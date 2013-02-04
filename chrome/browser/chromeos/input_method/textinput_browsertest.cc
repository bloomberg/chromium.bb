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

namespace chromeos {

class TextInputTest : public InProcessBrowserTest {
 public:
  TextInputTest() {}
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
};

IN_PROC_BROWSER_TEST_F(TextInputTest, SwitchToPasswordFieldTest) {
  GURL url = ui_test_utils::GetTestUrl(
      FilePath("textinput"),
      FilePath("ime_enable_disable_test.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  bool worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(text01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  content::WaitForLoadStop(tab);
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(GetInputMethod()->latest_text_input_type(),
            ui::TEXT_INPUT_TYPE_TEXT);

  worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(password01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  content::WaitForLoadStop(tab);
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(GetInputMethod()->latest_text_input_type(),
            ui::TEXT_INPUT_TYPE_PASSWORD);

}

} // namespace chromeos

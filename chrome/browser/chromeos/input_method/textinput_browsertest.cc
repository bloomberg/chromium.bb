// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef TextInputTestBase TextInput_TextInputStateChangedTest;

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       SwitchToPasswordFieldTest) {
  TextInputTestHelper helper;
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath("textinput"),
      base::FilePath("ime_enable_disable_test.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  bool worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(text01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());

  worker_finished = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(password01_focus());",
      &worker_finished));
  EXPECT_TRUE(worker_finished);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());
}

} // namespace chromeos

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef TextInputTestBase TextInput_TextInputStateChangedTest;

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       SwitchToPasswordFieldTest) {
  TextInputTestHelper helper;
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("ime_enable_disable_test.html")));
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

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest, FocusOnLoadTest) {
  TextInputTestHelper helper;
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_on_load.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       FocusOnContentJSTest) {
  TextInputTestHelper helper;
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_on_content_js.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       MouseClickChange) {
  TextInputTestHelper helper;
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_with_mouse_click.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ASSERT_TRUE(helper.ClickElement("text_id", tab));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, helper.GetTextInputType());

  ASSERT_TRUE(helper.ClickElement("password_id", tab));
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());
}

IN_PROC_BROWSER_TEST_F(TextInput_TextInputStateChangedTest,
                       FocusChangeOnFocus) {
  TextInputTestHelper helper;
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("focus_input_on_anothor_focus.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, helper.GetTextInputType());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string coordinate;
  ASSERT_TRUE(content::ExecuteScript(
          tab,
          "document.getElementById('text_id').focus();"));

  // Expects PASSWORD text input type because javascript will chagne the focus
  // to password field in #text_id's onfocus handler.
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());

  ASSERT_TRUE(helper.ClickElement("text_id", tab));
  // Expects PASSWORD text input type because javascript will chagne the focus
  // to password field in #text_id's onfocus handler.
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, helper.GetTextInputType());
}

} // namespace chromeos

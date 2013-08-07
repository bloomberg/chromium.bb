// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef TextInputTestBase KeyboardEventEndToEndTest;

// Flaky test: 268049
IN_PROC_BROWSER_TEST_F(KeyboardEventEndToEndTest,
                       DISABLED_AltGrToCtrlAltKeyDown) {
  TextInputTestHelper helper;

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("textinput")),
      base::FilePath(FILE_PATH_LITERAL("keyevent_logging.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  helper.ClickElement("text_id", tab);
  helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT);

  {
    ASSERT_TRUE(content::ExecuteScript(
        tab,
        "initKeyDownExpectations(["
        // Alt down has (only) altKey true in this case.
        "{ keyCode:18, shiftKey:false, ctrlKey:false, altKey:true }]);"));
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(),
                                                ui::VKEY_MENU,
                                                false,
                                                false,
                                                false,
                                                false));
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "didTestSucceed();",
        &result));
    EXPECT_TRUE(result);
  }
  {
    ASSERT_TRUE(content::ExecuteScript(
        tab,
        "initKeyDownExpectations(["
        // Ctrl down has (only) ctrlKey true in this case.
        "{ keyCode:17, shiftKey:false, ctrlKey:true, altKey:false }]);"));
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(),
                                                ui::VKEY_CONTROL,
                                                false,
                                                false,
                                                false,
                                                false));
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "didTestSucceed();",
        &result));
    EXPECT_TRUE(result);
  }
  {
    ASSERT_TRUE(content::ExecuteScript(
        tab,
        "initKeyDownExpectations(["
        // Ctrl down has ctrlKey false in this case.
        "{ keyCode:17, shiftKey:false , ctrlKey:false , altKey:false },"
        // Alt down has altKey false in this case.
        "{ keyCode:18, shiftKey:false , ctrlKey:false , altKey:false }]);"));
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(),
                                                ui::VKEY_ALTGR,
                                                false,
                                                false,
                                                false,
                                                false));
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "didTestSucceed();",
        &result));
    EXPECT_TRUE(result);
  }
}

// TODO(nona): Add AltGr modifier test. Need to add AltGr handling into
//             SendKeyPressSync(crbug.com/262928).

} // namespace chromeos

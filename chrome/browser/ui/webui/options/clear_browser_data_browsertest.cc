// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"
#include "chrome/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"

namespace options {

class ClearBrowserDataBrowserTest : public OptionsUIBrowserTest {
 protected:
  void ClickElement(const std::string& selector) {
    bool element_enabled = false;
    ASSERT_NO_FATAL_FAILURE(GetElementEnabledState(selector, &element_enabled));
    ASSERT_TRUE(element_enabled);
    ASSERT_TRUE(content::ExecuteScript(
        GetSettingsFrame(),
        "document.querySelector('" + selector + "').click();"));
  }

  bool IsElementEnabled(const std::string& selector) {
    bool element_enabled = false;
    GetElementEnabledState(selector, &element_enabled);
    return element_enabled;
  }

 private:
  void GetElementEnabledState(
      const std::string& selector,
      bool* enabled) {
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetSettingsFrame(),
        "window.domAutomationController.send(!document.querySelector('" +
            selector + "').disabled);",
        enabled));
  }
};

IN_PROC_BROWSER_TEST_F(ClearBrowserDataBrowserTest,
                       CommitButtonDisabledWhileDeletionInProgress) {
  const char kCommitButtonId[] = "#clear-browser-data-commit";
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor;

  // Navigate to the Clear Browsing Data dialog to ensure that the commit button
  // is initially enabled, usable, and gets disabled after having been pressed.
  NavigateToSettingsSubpage(chrome::kClearBrowserDataSubPage);
  ASSERT_NO_FATAL_FAILURE(ClickElement(kCommitButtonId));
  EXPECT_FALSE(IsElementEnabled(kCommitButtonId));

  completion_inhibitor.BlockUntilNearCompletion();

  // Simulate a reload while the previous removal is still running, and verify
  // that the button is still disabled.
  NavigateToSettingsSubpage(chrome::kClearBrowserDataSubPage);
  EXPECT_FALSE(IsElementEnabled(kCommitButtonId));

  completion_inhibitor.ContinueToCompletion();

  // However, the button should be enabled again once the process has finished.
  NavigateToSettingsSubpage(chrome::kClearBrowserDataSubPage);
  EXPECT_TRUE(IsElementEnabled(kCommitButtonId));
}

}  // namespace options

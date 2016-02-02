// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
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

// http://crbug.com/458684
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_CommitButtonDisabledWhileDeletionInProgress \
    DISABLED_CommitButtonDisabledWhileDeletionInProgress
#else
#define MAYBE_CommitButtonDisabledWhileDeletionInProgress \
    CommitButtonDisabledWhileDeletionInProgress
#endif

IN_PROC_BROWSER_TEST_F(ClearBrowserDataBrowserTest,
                       MAYBE_CommitButtonDisabledWhileDeletionInProgress) {
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

IN_PROC_BROWSER_TEST_F(ClearBrowserDataBrowserTest,
                       CommitButtonDisabledWhenNoDataTypesSelected) {
  const char kCommitButtonId[] = "#clear-browser-data-commit";
  const char* kDataTypes[] = {"browser.clear_data.browsing_history",
                              "browser.clear_data.download_history",
                              "browser.clear_data.cache",
                              "browser.clear_data.cookies",
                              "browser.clear_data.passwords",
                              "browser.clear_data.form_data",
                              "browser.clear_data.hosted_apps_data",
                              "browser.clear_data.content_licenses"};

  PrefService* prefs = browser()->profile()->GetPrefs();
  for (size_t i = 0; i < arraysize(kDataTypes); ++i) {
    prefs->SetBoolean(kDataTypes[i], false);
  }

  // Navigate to the Clear Browsing Data dialog to ensure that the commit button
  // is disabled if clearing is not requested for any of the data types.
  NavigateToSettingsSubpage(chrome::kClearBrowserDataSubPage);
  EXPECT_FALSE(IsElementEnabled(kCommitButtonId));

  // However, expect the commit button to be re-enabled if any of the data types
  // gets selected to be cleared.
  for (size_t i = 0; i < arraysize(kDataTypes); ++i) {
    prefs->SetBoolean(kDataTypes[i], true);
    EXPECT_TRUE(IsElementEnabled(kCommitButtonId));
    prefs->SetBoolean(kDataTypes[i], false);
  }
}

}  // namespace options

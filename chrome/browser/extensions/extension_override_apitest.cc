// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/test/ui_test_utils.h"

class ExtensionOverrideTest : public ExtensionApiTest {
 protected:
  bool CheckHistoryOverridesContainsNoDupes() {
    // There should be no duplicate entries in the preferences.
    const DictionaryValue* overrides =
        browser()->profile()->GetPrefs()->GetDictionary(
            ExtensionDOMUI::kExtensionURLOverrides);

    ListValue* values = NULL;
    if (!overrides->GetList(L"history", &values))
      return false;

    std::set<std::string> seen_overrides;
    for (size_t i = 0; i < values->GetSize(); ++i) {
      std::string value;
      if (!values->GetString(i, &value))
        return false;

      if (seen_overrides.find(value) != seen_overrides.end())
        return false;

      seen_overrides.insert(value);
    }

    return true;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewtab) {
  ASSERT_TRUE(RunExtensionTest("override/newtab")) << message_;
  {
    ResultCatcher catcher;
    // Navigate to the new tab page.  The overridden new tab page
    // will call chrome.test.notifyPass() .
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // TODO(erikkay) Load a second extension with the same override.
  // Verify behavior, then unload the first and verify behavior, etc.
}

IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideHistory) {
  ASSERT_TRUE(RunExtensionTest("override/history")) << message_;
  {
    ResultCatcher catcher;
    // Navigate to the history page.  The overridden history page
    // will call chrome.test.notifyPass() .
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://history/"));
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

// Regression test for http://crbug.com/41442.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, ShouldNotCreateDuplicateEntries) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("override/history")));

  // Simulate several LoadExtension() calls happening over the lifetime of
  // a preferences file without corresponding UnloadExtension() calls.
  for (size_t i = 0; i < 3; ++i) {
    ExtensionDOMUI::RegisterChromeURLOverrides(
        browser()->profile(),
        browser()->profile()->GetExtensionsService()->extensions()->back()->
            GetChromeURLOverrides());
  }

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, ShouldCleanUpDuplicateEntries) {
  // Simulate several LoadExtension() calls happening over the lifetime of
  // a preferences file without corresponding UnloadExtension() calls. This is
  // the same as the above test, except for that it is testing the case where
  // the file already contains dupes when an extension is loaded.
  ListValue* list = new ListValue();
  for (size_t i = 0; i < 3; ++i)
    list->Append(Value::CreateStringValue("http://www.google.com/"));

  browser()->profile()->GetPrefs()->GetMutableDictionary(
      ExtensionDOMUI::kExtensionURLOverrides)->Set(L"history", list);

  ASSERT_FALSE(CheckHistoryOverridesContainsNoDupes());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("override/history")));

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

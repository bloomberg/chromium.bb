// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"

using content::WebContents;

class ExtensionOverrideTest : public ExtensionApiTest {
 protected:
  bool CheckHistoryOverridesContainsNoDupes() {
    // There should be no duplicate entries in the preferences.
    const DictionaryValue* overrides =
        browser()->profile()->GetPrefs()->GetDictionary(
            ExtensionWebUI::kExtensionURLOverrides);

    const ListValue* values = NULL;
    if (!overrides->GetList("history", &values))
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
    WebContents* tab = chrome::GetActiveWebContents(browser());
    ASSERT_TRUE(tab->GetController().GetActiveEntry());
    EXPECT_TRUE(tab->GetController().GetActiveEntry()->GetURL().
                SchemeIs(extensions::kExtensionScheme));

    ASSERT_TRUE(catcher.GetNextResult());
  }

  // TODO(erikkay) Load a second extension with the same override.
  // Verify behavior, then unload the first and verify behavior, etc.
}

#if defined(OS_MACOSX)
// Hangy: http://crbug.com/70511
#define MAYBE_OverrideNewtabIncognito DISABLED_OverrideNewtabIncognito
#else
#define MAYBE_OverrideNewtabIncognito OverrideNewtabIncognito
#endif
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, MAYBE_OverrideNewtabIncognito) {
  ASSERT_TRUE(RunExtensionTest("override/newtab")) << message_;

  // Navigate an incognito tab to the new tab page.  We should get the actual
  // new tab page because we can't load chrome-extension URLs in incognito.
  Browser* otr_browser = ui_test_utils::OpenURLOffTheRecord(
      browser()->profile(), GURL("chrome://newtab/"));
  WebContents* tab = chrome::GetActiveWebContents(otr_browser);
  ASSERT_TRUE(tab->GetController().GetActiveEntry());
  EXPECT_FALSE(tab->GetController().GetActiveEntry()->GetURL().
               SchemeIs(extensions::kExtensionScheme));
}

// Times out consistently on Win, http://crbug.com/45173.
#if defined(OS_WIN)
#define MAYBE_OverrideHistory DISABLED_OverrideHistory
#else
#define MAYBE_OverrideHistory OverrideHistory
#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, MAYBE_OverrideHistory) {
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
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("override/history"));
  ASSERT_TRUE(extension);

  // Simulate several LoadExtension() calls happening over the lifetime of
  // a preferences file without corresponding UnloadExtension() calls.
  for (size_t i = 0; i < 3; ++i) {
    ExtensionWebUI::RegisterChromeURLOverrides(
        browser()->profile(),
        extensions::URLOverrides::GetChromeURLOverrides(extension));
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

  {
    DictionaryPrefUpdate update(browser()->profile()->GetPrefs(),
                                ExtensionWebUI::kExtensionURLOverrides);
    update.Get()->Set("history", list);
  }

  ASSERT_FALSE(CheckHistoryOverridesContainsNoDupes());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("override/history")));

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

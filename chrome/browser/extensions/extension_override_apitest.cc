// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

using content::WebContents;

namespace extensions {

class ExtensionOverrideTest : public ExtensionApiTest {
 protected:
  bool CheckHistoryOverridesContainsNoDupes() {
    // There should be no duplicate entries in the preferences.
    const base::DictionaryValue* overrides =
        browser()->profile()->GetPrefs()->GetDictionary(
            ExtensionWebUI::kExtensionURLOverrides);

    const base::ListValue* values = nullptr;
    if (!overrides->GetList("history", &values))
      return false;

    std::set<std::string> seen_overrides;
    for (const base::Value* val : *values) {
      const base::DictionaryValue* dict = nullptr;
      std::string entry;
      if (!val->GetAsDictionary(&dict) || !dict->GetString("entry", &entry) ||
          seen_overrides.count(entry) != 0)
        return false;
      seen_overrides.insert(entry);
    }

    return true;
  }

  // Returns AssertionSuccess() if the given |web_contents| is being actively
  // controlled by the extension with |extension_id|.
  testing::AssertionResult ExtensionControlsPage(
      content::WebContents* web_contents,
      const std::string& extension_id) {
    if (!web_contents->GetController().GetLastCommittedEntry())
      return testing::AssertionFailure() << "No last committed entry.";
    // We can't just use WebContents::GetLastCommittedURL() here because
    // trickiness makes it think that it committed chrome://newtab.
    GURL gurl = web_contents->GetController().GetLastCommittedEntry()->GetURL();
    if (!gurl.SchemeIs(kExtensionScheme))
      return testing::AssertionFailure() << gurl;
    if (gurl.host_piece() != extension_id)
      return testing::AssertionFailure() << gurl;
    return testing::AssertionSuccess();
  }

  base::FilePath data_dir() {
    return test_data_dir_.AppendASCII("override");
  }
};

// Basic test for overriding the NTP.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewTab) {
  const Extension* extension = LoadExtension(data_dir().AppendASCII("newtab"));
  {
    // Navigate to the new tab page.  The overridden new tab page
    // will call chrome.test.sendMessage('controlled by first').
    ExtensionTestMessageListener listener(false);
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/"));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension->id()));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by first", listener.message());
  }
}

// Check having multiple extensions with the same override.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewTabMultiple) {
  // Prefer IDs because loading/unloading invalidates the extension ptrs.
  const std::string extension1_id =
      LoadExtension(data_dir().AppendASCII("newtab"))->id();
  const std::string extension2_id =
      LoadExtension(data_dir().AppendASCII("newtab2"))->id();
  {
    // Navigate to the new tab page. Last extension installed wins, so
    // the new tab page should be controlled by the second extension.
    ExtensionTestMessageListener listener(false);
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/"));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  // Unload and reload the first extension. This should *not* result in the
  // first extension moving to the front of the line.
  ReloadExtension(extension1_id);

  {
    // The page should still be controlled by the second extension.
    ExtensionTestMessageListener listener(false);
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/"));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  // Unload the (controlling) second extension. Now, and only now, should
  // extension1 take over.
  UnloadExtension(extension2_id);

  {
    ExtensionTestMessageListener listener(false);
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/"));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension1_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by first", listener.message());
  }
}

// Test that unloading an extension overriding the page reloads the page with
// the proper url.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest,
                       OverridingExtensionUnloadedWithPageOpen) {
  // Prefer IDs because loading/unloading invalidates the extension ptrs.
  const std::string extension1_id =
      LoadExtension(data_dir().AppendASCII("newtab"))->id();
  const std::string extension2_id =
      LoadExtension(data_dir().AppendASCII("newtab2"))->id();
  {
    // Navigate to the new tab page. Last extension installed wins, so
    // the new tab page should be controlled by the second extension.
    ExtensionTestMessageListener listener(false);
    ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/"));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  {
    // Unload the controlling extension. The page should be automatically
    // reloaded with the new controlling extension.
    ExtensionTestMessageListener listener(false);
    UnloadExtension(extension2_id);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by first", listener.message());
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension1_id));
  }
}

#if defined(OS_MACOSX)
// Hangy: http://crbug.com/70511
#define MAYBE_OverrideNewTabIncognito DISABLED_OverrideNewTabIncognito
#else
#define MAYBE_OverrideNewTabIncognito OverrideNewTabIncognito
#endif
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, MAYBE_OverrideNewTabIncognito) {
  LoadExtension(data_dir().AppendASCII("newtab"));

  // Navigate an incognito tab to the new tab page.  We should get the actual
  // new tab page because we can't load chrome-extension URLs in incognito.
  Browser* otr_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("chrome://newtab/"));
  WebContents* tab = otr_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab->GetController().GetVisibleEntry());
  EXPECT_FALSE(tab->GetController().GetVisibleEntry()->GetURL().
               SchemeIs(kExtensionScheme));
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
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("override/history"));
  ASSERT_TRUE(extension);

  // Simulate several LoadExtension() calls happening over the lifetime of
  // a preferences file without corresponding UnloadExtension() calls.
  for (size_t i = 0; i < 3; ++i) {
    ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
        browser()->profile(),
        URLOverrides::GetChromeURLOverrides(extension));
  }

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

// TODO(devlin): This test seems a bit contrived. How would we end up with
// duplicate entries created?
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, ShouldCleanUpDuplicateEntries) {
  // Simulate several LoadExtension() calls happening over the lifetime of
  // a preferences file without corresponding UnloadExtension() calls. This is
  // the same as the above test, except for that it is testing the case where
  // the file already contains dupes when an extension is loaded.
  base::ListValue* list = new base::ListValue();
  for (size_t i = 0; i < 3; ++i) {
    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("entry", "http://www.google.com/");
    dict->SetBoolean("active", true);
    list->Append(std::move(dict));
  }

  {
    DictionaryPrefUpdate update(browser()->profile()->GetPrefs(),
                                ExtensionWebUI::kExtensionURLOverrides);
    update.Get()->Set("history", list);
  }

  ASSERT_FALSE(CheckHistoryOverridesContainsNoDupes());

  ExtensionWebUI::InitializeChromeURLOverrides(profile());

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

}  // namespace extensions

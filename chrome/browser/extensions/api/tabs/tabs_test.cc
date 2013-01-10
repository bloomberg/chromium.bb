// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/gfx/rect.h"

namespace keys = extensions::tabs_constants;
namespace utils = extension_function_test_utils;

namespace {

class ExtensionTabsTest : public InProcessBrowserTest {
};

}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Invalid window ID error.
  scoped_refptr<GetWindowFunction> function = new GetWindowFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf("[%u]", window_id + 1),
          browser()),
      keys::kWindowNotFoundError));

  // Basic window details.
  gfx::Rect bounds;
  if (browser()->window()->IsMinimized())
    bounds = browser()->window()->GetRestoredBounds();
  else
    bounds = browser()->window()->GetBounds();

  function = new GetWindowFunction();
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u]", window_id),
          browser())));
  EXPECT_EQ(window_id, utils::GetInteger(result.get(), "id"));
  EXPECT_FALSE(utils::GetBoolean(result.get(), "incognito"));
  EXPECT_EQ("normal", utils::GetString(result.get(), "type"));
  EXPECT_EQ(bounds.x(), utils::GetInteger(result.get(), "left"));
  EXPECT_EQ(bounds.y(), utils::GetInteger(result.get(), "top"));
  EXPECT_EQ(bounds.width(), utils::GetInteger(result.get(), "width"));
  EXPECT_EQ(bounds.height(), utils::GetInteger(result.get(), "height"));

  // With "populate" enabled.
  function = new GetWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u, {\"populate\": true}]", window_id),
          browser())));

  EXPECT_EQ(window_id, utils::GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  ListValue* tabs = NULL;
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));

  // TODO(aa): Can't assume window is focused. On mac, calling Activate() from a
  // browser test doesn't seem to do anything, so can't test the opposite
  // either.
  EXPECT_EQ(browser()->window()->IsActive(),
            utils::GetBoolean(result.get(), "focused"));

  // TODO(aa): Minimized and maximized dimensions. Is there a way to set
  // minimize/maximize programmatically?

  // Popup.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  function = new GetWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(popup_browser)),
          browser())));
  EXPECT_EQ("popup", utils::GetString(result.get(), "type"));

  // Panel.
  Browser* panel_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_PANEL, browser()->profile()));
  function = new GetWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(panel_browser)),
          browser())));
  EXPECT_EQ("panel", utils::GetString(result.get(), "type"));

  // Incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  int incognito_window_id = ExtensionTabUtil::GetWindowId(incognito_browser);

  // Without "include_incognito".
  function = new GetWindowFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser()),
      keys::kWindowNotFoundError));

  // With "include_incognito".
  function = new GetWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser(),
          utils::INCLUDE_INCOGNITO)));
  EXPECT_TRUE(utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetCurrentWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());
  Browser* new_browser = CreateBrowser(browser()->profile());
  int new_id = ExtensionTabUtil::GetWindowId(new_browser);

  // Get the current window using new_browser.
  scoped_refptr<GetCurrentWindowFunction> function =
      new GetCurrentWindowFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[]",
                                              new_browser)));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(new_id, utils::GetInteger(result.get(), "id"));
  ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  // Get the current window using the old window and make the tabs populated.
  function = new GetCurrentWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(window_id, utils::GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetAllWindows) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  scoped_refptr<GetAllWindowsFunction> function = new GetAllWindowsFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[]",
                                              browser())));

  ListValue* windows = result.get();
  EXPECT_EQ(NUM_WINDOWS, windows->GetSize());
  for (size_t i = 0; i < NUM_WINDOWS; ++i) {
    DictionaryValue* result_window = NULL;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(utils::GetInteger(result_window, "id"));

    // "populate" was not passed in so tabs are not populated.
    ListValue* tabs = NULL;
    EXPECT_FALSE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  function = new GetAllWindowsFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  windows = result.get();
  EXPECT_EQ(NUM_WINDOWS, windows->GetSize());
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    DictionaryValue* result_window = NULL;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(utils::GetInteger(result_window, "id"));

    // "populate" was enabled so tabs should be populated.
    ListValue* tabs = NULL;
    EXPECT_TRUE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, UpdateNoPermissions) {
  // The test empty extension has no permissions, therefore it should not get
  // tab data in the function result.
  scoped_refptr<UpdateTabFunction> update_tab_function(new UpdateTabFunction());
  scoped_refptr<extensions::Extension> empty_extension(
      utils::CreateEmptyExtension());
  update_tab_function->set_extension(empty_extension.get());
  // Without a callback the function will not generate a result.
  update_tab_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          update_tab_function.get(),
          "[null, {\"url\": \"about:blank\", \"pinned\": true}]",
          browser())));
  // The url is stripped since the extension does not have tab permissions.
  EXPECT_FALSE(result->HasKey("url"));
  EXPECT_TRUE(utils::GetBoolean(result.get(), "pinned"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForced) {
  static const char kArgsWithoutExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\"}]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_refptr<CreateWindowFunction> function(new CreateWindowFunction());
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          kArgsWithoutExplicitIncognitoParam,
          browser(),
          utils::INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(utils::GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = new CreateWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          kArgsWithoutExplicitIncognitoParam,
          incognito_browser,
          utils::INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForcedAndNoArgs) {
  static const char kEmptyArgs[] = "[]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_refptr<CreateWindowFunction> function = new CreateWindowFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              kEmptyArgs,
                                              browser(),
                                              utils::INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(utils::GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = new CreateWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              kEmptyArgs,
                                              incognito_browser,
                                              utils::INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            utils::GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateNormalWindowWhenIncognitoForced) {
  static const char kArgsWithExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\", \"incognito\": false }]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);

  // Run with an explicit "incognito" param.
  scoped_refptr<CreateWindowFunction> function = new CreateWindowFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgsWithExplicitIncognitoParam,
                                       browser()),
      keys::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  function = new CreateWindowFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgsWithExplicitIncognitoParam,
                                       incognito_browser),
      keys::kIncognitoModeIsForced));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateIncognitoWindowWhenIncognitoDisabled) {
  static const char kArgs[] =
      "[{\"url\": \"about:blank\", \"incognito\": true }]";

  Browser* incognito_browser = CreateIncognitoBrowser();
  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  // Run in normal window.
  scoped_refptr<CreateWindowFunction> function = new CreateWindowFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgs,
                                       browser()),
      keys::kIncognitoModeIsDisabled));

  // Run in incognito window.
  function = new CreateWindowFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgs,
                                       incognito_browser),
      keys::kIncognitoModeIsDisabled));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryCurrentWindowTabs) {
  const size_t kExtraWindows = 3;
  for (size_t i = 0; i < kExtraWindows; ++i)
    CreateBrowser(browser()->profile());

  GURL url;
  AddTabAtIndexToBrowser(browser(), 0, url, content::PAGE_TRANSITION_LINK);
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Get tabs in the 'current' window called from non-focused browser.
  scoped_refptr<QueryTabsFunction> function = new QueryTabsFunction();
  function->set_extension(utils::CreateEmptyExtension().get());
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"currentWindow\":true}]",
                                              browser())));

  ListValue* result_tabs = result.get();
  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_EQ(window_id, utils::GetInteger(result_tab, keys::kWindowIdKey));
  }

  // Get tabs NOT in the 'current' window called from non-focused browser.
  function = new QueryTabsFunction();
  function->set_extension(utils::CreateEmptyExtension().get());
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"currentWindow\":false}]",
                                              browser())));

  result_tabs = result.get();
  // We should have one tab for each extra window.
  EXPECT_EQ(kExtraWindows, result_tabs->GetSize());
  for (size_t i = 0; i < kExtraWindows; ++i) {
    DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_NE(window_id, utils::GetInteger(result_tab, keys::kWindowIdKey));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DontCreateTabInClosingPopupWindow) {
  // Test creates new popup window, closes it right away and then tries to open
  // a new tab in it. Tab should not be opened in the popup window, but in a
  // tabbed browser window.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  int window_id = ExtensionTabUtil::GetWindowId(popup_browser);
  chrome::CloseWindow(popup_browser);

  scoped_refptr<CreateTabFunction> create_tab_function(new CreateTabFunction());
  create_tab_function->set_extension(utils::CreateEmptyExtension().get());
  // Without a callback the function will not generate a result.
  create_tab_function->set_has_callback(true);

  static const char kNewBlankTabArgs[] =
      "[{\"url\": \"about:blank\", \"windowId\": %u}]";

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          create_tab_function.get(),
          base::StringPrintf(kNewBlankTabArgs, window_id),
          browser())));

  EXPECT_NE(window_id, utils::GetInteger(result.get(), "windowId"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowState) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  scoped_refptr<UpdateWindowFunction> function = new UpdateWindowFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  function = new UpdateWindowFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  function = new UpdateWindowFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  function = new UpdateWindowFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTab) {
  static const char kNewBlankTabArgs[] ="about:blank";

  content::OpenURLParams params(GURL(kNewBlankTabArgs), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  scoped_refptr<DuplicateTabFunction> duplicate_tab_function(
      new DuplicateTabFunction());
  scoped_ptr<base::DictionaryValue> test_extension_value(
      utils::ParseDictionary(
      "{\"name\": \"Test\", \"version\": \"1.0\", \"permissions\": [\"tabs\"]}"
      ));
  scoped_refptr<extensions::Extension> empty_tab_extension(
      utils::CreateExtension(test_extension_value.get()));
  duplicate_tab_function->set_extension(empty_tab_extension.get());
  duplicate_tab_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> duplicate_result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  int duplicate_tab_id = utils::GetInteger(duplicate_result.get(), "id");
  int duplicate_tab_window_id = utils::GetInteger(duplicate_result.get(),
                                                  "windowId");
  int duplicate_tab_index = utils::GetInteger(duplicate_result.get(), "index");
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, duplicate_result->GetType());
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty tab extension has tabs permissions, therefore
  // |duplicate_result| should contain url, title, and faviconUrl
  // in the function result.
  EXPECT_TRUE(utils::HasPrivacySensitiveFields(duplicate_result.get()));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTabNoPermission) {
  static const char kNewBlankTabArgs[] ="about:blank";

  content::OpenURLParams params(GURL(kNewBlankTabArgs), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  scoped_refptr<DuplicateTabFunction> duplicate_tab_function(
      new DuplicateTabFunction());
  scoped_refptr<extensions::Extension> empty_extension(
      utils::CreateEmptyExtension());
  duplicate_tab_function->set_extension(empty_extension.get());
  duplicate_tab_function->set_has_callback(true);

  scoped_ptr<base::DictionaryValue> duplicate_result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  int duplicate_tab_id = utils::GetInteger(duplicate_result.get(), "id");
  int duplicate_tab_window_id = utils::GetInteger(duplicate_result.get(),
                                                  "windowId");
  int duplicate_tab_index = utils::GetInteger(duplicate_result.get(), "index");
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, duplicate_result->GetType());
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty extension has no permissions, therefore |duplicate_result|
  // should not contain url, title, and faviconUrl in the function result.
  EXPECT_FALSE(utils::HasPrivacySensitiveFields(duplicate_result.get()));
}

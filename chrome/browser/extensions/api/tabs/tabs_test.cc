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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new GetWindowFunction(),
          base::StringPrintf("[%u]", window_id + 1),
          browser()),
      keys::kWindowNotFoundError));

  // Basic window details.
  gfx::Rect bounds;
  if (browser()->window()->IsMinimized())
    bounds = browser()->window()->GetRestoredBounds();
  else
    bounds = browser()->window()->GetBounds();

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetWindowFunction(),
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
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetWindowFunction(),
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
  Browser* popup_browser = Browser::CreateWithParams(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(popup_browser)),
          browser())));
  EXPECT_EQ("popup", utils::GetString(result.get(), "type"));

  // Panel.
  Browser* panel_browser = Browser::CreateWithParams(
      Browser::CreateParams(Browser::TYPE_PANEL, browser()->profile()));
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(panel_browser)),
          browser())));
  EXPECT_EQ("panel", utils::GetString(result.get(), "type"));

  // Incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  int incognito_window_id = ExtensionTabUtil::GetWindowId(incognito_browser);

  // Without "include_incognito".
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new GetWindowFunction(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser()),
      keys::kWindowNotFoundError));

  // With "include_incognito".
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetWindowFunction(),
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
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetCurrentWindowFunction(),
          "[]",
          new_browser)));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnResult.
  EXPECT_EQ(new_id, utils::GetInteger(result.get(), "id"));
  ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  // Get the current window using the old window and make the tabs populated.
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetCurrentWindowFunction(),
          "[{\"populate\": true}]",
          browser())));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnResult.
  EXPECT_EQ(window_id, utils::GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetLastFocusedWindow) {
  // Create a new window which making it the "last focused" window.
  // Note that "last focused" means the "top" most window.
  Browser* new_browser = CreateBrowser(browser()->profile());
  int focused_window_id = ExtensionTabUtil::GetWindowId(new_browser);

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetLastFocusedWindowFunction(),
          "[]",
          new_browser)));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnResult.
  EXPECT_EQ(focused_window_id, utils::GetInteger(result.get(), "id"));
  ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new GetLastFocusedWindowFunction(),
          "[{\"populate\": true}]",
          browser())));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnResult.
  EXPECT_EQ(focused_window_id, utils::GetInteger(result.get(), "id"));
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

  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnResult(
          new GetAllWindowsFunction(),
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
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnResult(
          new GetAllWindowsFunction(),
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

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnResult(
          update_tab_function.get(),
          "[null, {\"url\": \"neutrinos\"}]",
          browser()));
  EXPECT_EQ(base::Value::TYPE_NULL, result->GetType());
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForced) {
  static const char kArgsWithoutExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\"}]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new CreateWindowFunction(),
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
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new CreateWindowFunction(),
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
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new CreateWindowFunction(),
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
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          new CreateWindowFunction(),
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
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new CreateWindowFunction(),
          kArgsWithExplicitIncognitoParam,
          browser()),
      keys::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new CreateWindowFunction(),
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
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new CreateWindowFunction(),
          kArgs,
          browser()),
      keys::kIncognitoModeIsDisabled));

  // Run in incognito window.
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new CreateWindowFunction(),
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
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnResult(
          new QueryTabsFunction(),
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
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnResult(
          new QueryTabsFunction(),
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

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryLastFocusedWindowTabs) {
  const size_t kExtraWindows = 2;
  for (size_t i = 0; i < kExtraWindows; ++i)
    CreateBrowser(browser()->profile());

  Browser* focused_window = CreateBrowser(browser()->profile());
#if defined(OS_MACOSX)
  // See BrowserWindowCocoa::Show. In tests, Browser::window()->IsActive won't
  // work unless we fake the browser being launched by the user.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      focused_window->window()->GetNativeWindow()));
#endif

  // Needed on Mac and Linux so that the BrowserWindow::IsActive calls work.
  ui_test_utils::RunAllPendingInMessageLoop();

  GURL url;
  AddTabAtIndexToBrowser(focused_window, 0, url, content::PAGE_TRANSITION_LINK);
  int focused_window_id = ExtensionTabUtil::GetWindowId(focused_window);

  // Get tabs in the 'last focused' window called from non-focused browser.
  scoped_ptr<base::ListValue> result(utils::ToList(
      utils::RunFunctionAndReturnResult(
          new QueryTabsFunction(),
          "[{\"lastFocusedWindow\":true}]",
          browser())));

  ListValue* result_tabs = result.get();
  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_EQ(focused_window_id, utils::GetInteger(result_tab,
                                                   keys::kWindowIdKey));
  }

  // Get tabs NOT in the 'last focused' window called from the focused browser.
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnResult(
          new QueryTabsFunction(),
          "[{\"lastFocusedWindow\":false}]",
          browser())));

  result_tabs = result.get();
  // We should get one tab for each extra window and one for the initial window.
  EXPECT_EQ(kExtraWindows + 1, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    DictionaryValue* result_tab = NULL;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_NE(focused_window_id, utils::GetInteger(result_tab,
                                                   keys::kWindowIdKey));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DontCreateTabInClosingPopupWindow) {
  // Test creates new popup window, closes it right away and then tries to open
  // a new tab in it. Tab should not be opened in the popup window, but in a
  // tabbed browser window.
  Browser* popup_browser = Browser::CreateWithParams(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  int window_id = ExtensionTabUtil::GetWindowId(popup_browser);
  popup_browser->CloseWindow();

  scoped_refptr<CreateTabFunction> create_tab_function(new CreateTabFunction());
  // Without a callback the function will not generate a result.
  create_tab_function->set_has_callback(true);

  static const char kNewBlankTabArgs[] =
      "[{\"url\": \"about:blank\", \"windowId\": %u}]";

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          create_tab_function.get(),
          base::StringPrintf(kNewBlankTabArgs, window_id),
          browser())));

  EXPECT_NE(window_id, utils::GetInteger(result.get(), "windowId"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowState) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  EXPECT_TRUE(MatchPattern(
      utils::RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));
}

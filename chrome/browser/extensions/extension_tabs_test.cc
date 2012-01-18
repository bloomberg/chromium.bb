// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tabs_module.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/gfx/rect.h"

using namespace extension_function_test_utils;
namespace keys = extension_tabs_module_constants;

namespace {

class ExtensionTabsTest : public InProcessBrowserTest {
};
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Invalid window ID error.
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new GetWindowFunction(),
          base::StringPrintf("[%u]", window_id + 1),
          browser()),
      keys::kWindowNotFoundError));

  // Basic window details.
  // Need GetRestoredBound instead of GetBounds.
  // See crbug.com/98759.
  gfx::Rect bounds = browser()->window()->GetRestoredBounds();

  scoped_ptr<base::DictionaryValue> result(ToDictionary(
      RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf("[%u]", window_id),
          browser())));
  EXPECT_EQ(window_id, GetInteger(result.get(), "id"));
  EXPECT_FALSE(GetBoolean(result.get(), "incognito"));
  EXPECT_EQ("normal", GetString(result.get(), "type"));
  EXPECT_EQ(bounds.x(), GetInteger(result.get(), "left"));
  EXPECT_EQ(bounds.y(), GetInteger(result.get(), "top"));
  EXPECT_EQ(bounds.width(), GetInteger(result.get(), "width"));
  EXPECT_EQ(bounds.height(), GetInteger(result.get(), "height"));

  // With "populate" enabled.
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf("[%u, {\"populate\": true}]", window_id),
          browser())));

  EXPECT_EQ(window_id, GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  ListValue* tabs = NULL;
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));

  // TODO(aa): Can't assume window is focused. On mac, calling Activate() from a
  // browser test doesn't seem to do anything, so can't test the opposite
  // either.
  EXPECT_EQ(browser()->window()->IsActive(),
            GetBoolean(result.get(), "focused"));

  // TODO(aa): Minimized and maximized dimensions. Is there a way to set
  // minimize/maximize programmatically?

  // Popup.
  Browser* popup_browser =
      Browser::CreateForType(Browser::TYPE_POPUP, browser()->profile());
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(popup_browser)),
          browser())));
  EXPECT_EQ("popup", GetString(result.get(), "type"));

  // Panel.
  Browser* panel_browser =
      Browser::CreateForType(Browser::TYPE_PANEL, browser()->profile());
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(panel_browser)),
          browser())));
  EXPECT_EQ("panel", GetString(result.get(), "type"));

  // Incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  int incognito_window_id = ExtensionTabUtil::GetWindowId(incognito_browser);

  // Without "include_incognito".
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new GetWindowFunction(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser()),
      keys::kWindowNotFoundError));

  // With "include_incognito".
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser(),
          INCLUDE_INCOGNITO)));
  EXPECT_TRUE(GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetCurrentWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());
  Browser* new_browser = CreateBrowser(browser()->profile());
  int new_id = ExtensionTabUtil::GetWindowId(new_browser);

  // Get the current window using new_browser.
  scoped_ptr<base::DictionaryValue> result(ToDictionary(
      RunFunctionAndReturnResult(
          new GetCurrentWindowFunction(),
          "[]",
          new_browser)));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnResult.
  EXPECT_EQ(new_id, GetInteger(result.get(), "id"));
  ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  // Get the current window using the old window and make the tabs populated.
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new GetCurrentWindowFunction(),
          "[{\"populate\": true}]",
          browser())));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnResult.
  EXPECT_EQ(window_id, GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetLastFocusedWindow) {
  // Create a new window which making it the "last focused" window.
  // Note that "last focused" means the "top" most window.
  Browser* new_browser = CreateBrowser(browser()->profile());
  int focused_window_id = ExtensionTabUtil::GetWindowId(new_browser);

  scoped_ptr<base::DictionaryValue> result(ToDictionary(
      RunFunctionAndReturnResult(
          new GetLastFocusedWindowFunction(),
          "[]",
          new_browser)));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnResult.
  EXPECT_EQ(focused_window_id, GetInteger(result.get(), "id"));
  ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new GetLastFocusedWindowFunction(),
          "[{\"populate\": true}]",
          browser())));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnResult.
  EXPECT_EQ(focused_window_id, GetInteger(result.get(), "id"));
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

  scoped_ptr<base::ListValue> result(ToList(
      RunFunctionAndReturnResult(
          new GetAllWindowsFunction(),
          "[]",
          browser())));

  ListValue* windows = result.get();
  EXPECT_EQ(NUM_WINDOWS, windows->GetSize());
  for (size_t i = 0; i < NUM_WINDOWS; ++i) {
    DictionaryValue* result_window = NULL;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(GetInteger(result_window, "id"));

    // "populate" was not passed in so tabs are not populated.
    ListValue* tabs = NULL;
    EXPECT_FALSE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  result.reset(ToList(
      RunFunctionAndReturnResult(
          new GetAllWindowsFunction(),
          "[{\"populate\": true}]",
          browser())));

  windows = result.get();
  EXPECT_EQ(NUM_WINDOWS, windows->GetSize());
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    DictionaryValue* result_window = NULL;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(GetInteger(result_window, "id"));

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
  scoped_refptr<Extension> empty_extension(CreateEmptyExtension());
  update_tab_function->set_extension(empty_extension.get());
  // Without a callback the function will not generate a result.
  update_tab_function->set_has_callback(true);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
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
  scoped_ptr<base::DictionaryValue> result(ToDictionary(
      RunFunctionAndReturnResult(
          new CreateWindowFunction(),
          kArgsWithoutExplicitIncognitoParam,
          browser(),
          INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new CreateWindowFunction(),
          kArgsWithoutExplicitIncognitoParam,
          incognito_browser,
          INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForcedAndNoArgs) {
  static const char kEmptyArgs[] = "[]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_ptr<base::DictionaryValue> result(ToDictionary(
      RunFunctionAndReturnResult(
          new CreateWindowFunction(),
          kEmptyArgs,
          browser(),
          INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new CreateWindowFunction(),
          kEmptyArgs,
          incognito_browser,
          INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            GetInteger(result.get(), "id"));
  // ... and it is incognito.
  EXPECT_TRUE(GetBoolean(result.get(), "incognito"));
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
      RunFunctionAndReturnError(
          new CreateWindowFunction(),
          kArgsWithExplicitIncognitoParam,
          browser()),
      keys::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
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
      RunFunctionAndReturnError(
          new CreateWindowFunction(),
          kArgs,
          browser()),
      keys::kIncognitoModeIsDisabled));

  // Run in incognito window.
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new CreateWindowFunction(),
          kArgs,
          incognito_browser),
      keys::kIncognitoModeIsDisabled));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowState) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

#if !defined(USE_AURA)
  // Disabled for now (crbug.com/105173) because window minimization is not
  // supported on Aura yet (crbug.com/104571).
  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));
#endif

  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id),
          browser()),
      keys::kInvalidWindowStateError));

#if !defined(USE_AURA)
  // Disabled for now (crbug.com/105173) because window minimization is not
  // supported on Aura yet (crbug.com/104571).
  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));
#endif

  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id),
          browser()),
      keys::kInvalidWindowStateError));
}

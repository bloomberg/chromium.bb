// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
      extension_tabs_module_constants::kWindowNotFoundError));

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
      extension_tabs_module_constants::kWindowNotFoundError));

  // With "include_incognito".
  result.reset(ToDictionary(
      RunFunctionAndReturnResult(
          new GetWindowFunction(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser(),
          INCLUDE_INCOGNITO)));
  EXPECT_TRUE(GetBoolean(result.get(), "incognito"));
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
      extension_tabs_module_constants::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new CreateWindowFunction(),
          kArgsWithExplicitIncognitoParam,
          incognito_browser),
      extension_tabs_module_constants::kIncognitoModeIsForced));
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
      extension_tabs_module_constants::kIncognitoModeIsDisabled));

  // Run in incognito window.
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new CreateWindowFunction(),
          kArgs,
          incognito_browser),
      extension_tabs_module_constants::kIncognitoModeIsDisabled));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowState) {
  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id),
          browser()),
      extension_tabs_module_constants::kInvalidWindowStateError));

  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id),
          browser()),
      extension_tabs_module_constants::kInvalidWindowStateError));

  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id),
          browser()),
      extension_tabs_module_constants::kInvalidWindowStateError));

  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new UpdateWindowFunction(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id),
          browser()),
      extension_tabs_module_constants::kInvalidWindowStateError));
}

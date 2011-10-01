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
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
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

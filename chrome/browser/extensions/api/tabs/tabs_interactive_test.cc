// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace keys = extensions::tabs_constants;
namespace utils = extension_function_test_utils;

typedef InProcessBrowserTest ExtensionTabsTest;

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetLastFocusedWindow) {
  // Create a new window which making it the "last focused" window.
  // Note that "last focused" means the "top" most window.
  Browser* new_browser = CreateBrowser(browser()->profile());
  int focused_window_id = ExtensionTabUtil::GetWindowId(new_browser);

  scoped_refptr<GetLastFocusedWindowFunction> function =
      new GetLastFocusedWindowFunction();
  scoped_refptr<extensions::Extension> extension(utils::CreateEmptyExtension());
  function->set_extension(extension.get());
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[]",
                                              new_browser)));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, utils::GetInteger(result.get(), "id"));
  ListValue* tabs = NULL;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  function = new GetLastFocusedWindowFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, utils::GetInteger(result.get(), "id"));
  // "populate" was enabled so tabs should be populated.
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));
}

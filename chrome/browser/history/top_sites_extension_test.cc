// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/history/top_sites_extension_api.h"
#include "chrome/test/base/in_process_browser_test.h"

using namespace extension_function_test_utils;

namespace {

class TopSitesExtensionTest : public InProcessBrowserTest {
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TopSitesExtensionTest, GetTopSites) {
  scoped_refptr<GetTopSitesFunction> get_top_sites_function(
      new GetTopSitesFunction());
  // Without a callback the function will not generate a result.
  get_top_sites_function->set_has_callback(true);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      get_top_sites_function.get(), "[]", browser()));
  EXPECT_EQ(base::Value::TYPE_LIST, result->GetType());
  // This should return at least 2 items (the prepopulated items).
  // TODO(estade): change 2 to arraylen(kPrepopulatedPages) after that
  // patch lands.
  EXPECT_GE(result->GetType(), 2);
}

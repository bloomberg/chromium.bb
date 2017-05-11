// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using WebUIBrowserTest = ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, ViewCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));
  int result = -1;
  NavigateToURL(shell(), GURL(kChromeUINetworkViewCacheURL));
  std::string script(
      "document.documentElement.innerHTML.indexOf('simple_page.html')");
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      shell()->web_contents(), "domAutomationController.send(" + script + ")",
      &result));
  ASSERT_NE(-1, result);
}

}  // namespace content

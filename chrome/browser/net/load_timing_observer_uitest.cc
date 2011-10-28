// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class LoadTimingObserverUITest : public UITest {
 public:
  LoadTimingObserverUITest()
      : http_server_(net::TestServer::TYPE_HTTP, FilePath()) {
    dom_automation_enabled_ = true;
  }

 protected:
  net::TestServer http_server_;
};

// http://crbug.com/102030
TEST_F(LoadTimingObserverUITest, FLAKY_CacheHitAfterRedirect) {
  ASSERT_TRUE(http_server_.Start());
  GURL cached_page = http_server_.GetURL("cachetime");
  std::string redirect = "server-redirect?" + cached_page.spec();
  NavigateToURL(cached_page);
  NavigateToURL(http_server_.GetURL(redirect));
  scoped_refptr<TabProxy> tab_proxy = GetActiveTab();
  int response_start = 0;
  int response_end = 0;
  ASSERT_TRUE(tab_proxy->ExecuteAndExtractInt(
      L"", L"window.domAutomationController.send("
      L"window.performance.timing.responseStart - "
      L"window.performance.timing.navigationStart)", &response_start));
  ASSERT_TRUE(tab_proxy->ExecuteAndExtractInt(
      L"", L"window.domAutomationController.send("
      L"window.performance.timing.responseEnd - "
      L"window.performance.timing.navigationStart)", &response_end));
  EXPECT_LE(response_start, response_end);
}

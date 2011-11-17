// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class LoadtimesExtensionBindingsUITest : public UITest {
 public:
  LoadtimesExtensionBindingsUITest()
      : http_server_(net::TestServer::TYPE_HTTP, FilePath()) {
    dom_automation_enabled_ = true;
  }

  void CompareBeforeAndAfter(TabProxy* tab_proxy) {
    // TODO(simonjam): There's a race on whether or not first paint is populated
    // before we read them. We ought to test that too. Until the race is fixed,
    // zero it out so the test is stable.
    ASSERT_TRUE(tab_proxy->ExecuteJavaScript(
        "window.before.firstPaintAfterLoadTime = 0;"
        "window.before.firstPaintTime = 0;"
        "window.after.firstPaintAfterLoadTime = 0;"
        "window.after.firstPaintTime = 0;"));

    std::wstring before;
    std::wstring after;
    ASSERT_TRUE(tab_proxy->ExecuteAndExtractString(
        L"", L"window.domAutomationController.send("
        L"JSON.stringify(before))", &before));
    ASSERT_TRUE(tab_proxy->ExecuteAndExtractString(
        L"", L"window.domAutomationController.send("
        L"JSON.stringify(after))", &after));
    EXPECT_EQ(before, after);
  }

 protected:
  net::TestServer http_server_;
};

TEST_F(LoadtimesExtensionBindingsUITest,
       LoadTimesSameAfterClientInDocNavigation) {
  ASSERT_TRUE(http_server_.Start());
  GURL plain_url = http_server_.GetURL("blank");
  NavigateToURL(plain_url);
  scoped_refptr<TabProxy> tab_proxy = GetActiveTab();
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->ExecuteJavaScript(
      "window.before = window.chrome.loadTimes()"));
  ASSERT_TRUE(tab_proxy->ExecuteJavaScript(
      "window.location.href = window.location + \"#\""));
  ASSERT_TRUE(tab_proxy->ExecuteJavaScript(
      "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter(tab_proxy.get());
}

TEST_F(LoadtimesExtensionBindingsUITest,
       LoadTimesSameAfterUserInDocNavigation) {
  ASSERT_TRUE(http_server_.Start());
  GURL plain_url = http_server_.GetURL("blank");
  GURL hash_url(plain_url.spec() + "#");
  NavigateToURL(plain_url);
  scoped_refptr<TabProxy> tab_proxy = GetActiveTab();
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->ExecuteJavaScript(
      "window.before = window.chrome.loadTimes()"));
  NavigateToURL(hash_url);
  ASSERT_TRUE(tab_proxy->ExecuteJavaScript(
      "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter(tab_proxy.get());
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

class HostRulesTest : public UITest {
 protected:
  HostRulesTest();

  net::TestServer test_server_;
  bool test_server_started_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostRulesTest);
};

HostRulesTest::HostRulesTest()
    : test_server_(net::TestServer::TYPE_HTTP,
                   FilePath(FILE_PATH_LITERAL("chrome/test/data"))),
      test_server_started_(false) {
  dom_automation_enabled_  = true;

  // The test_server is started in the constructor (rather than the test body)
  // so the mapping rules below can include the ephemeral port number.
  // TODO(phajdan.jr): Change this code when we can ask the test server whether
  // it started.
  test_server_started_ = test_server_.Start();
  if (!test_server_started_)
    return;

  // Map all hosts to our local server.
  std::string host_rule("MAP * " + test_server_.host_port_pair().ToString());
  launch_arguments_.AppendSwitchASCII(switches::kHostRules, host_rule);
}

TEST_F(HostRulesTest, TestMap) {
  ASSERT_TRUE(test_server_started_);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab(browser->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Go to the empty page using www.google.com as the host.
  GURL local_url = test_server_.GetURL("files/empty.html");
  GURL test_url(std::string("http://www.google.com") + local_url.path());
  EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(test_url));

  std::wstring html;
  EXPECT_TRUE(tab->ExecuteAndExtractString(
      L"",
      L"window.domAutomationController.send(document.body.outerHTML);",
      &html));

  EXPECT_STREQ(L"<body></body>", html.c_str());
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/test_server.h"

class HostRulesTest : public InProcessBrowserTest {
 protected:
  HostRulesTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    ASSERT_TRUE(test_server()->Start());

    // Map all hosts to our local server.
    std::string host_rule(
        "MAP * " + test_server()->host_port_pair().ToString());
    command_line->AppendSwitchASCII(switches::kHostRules, host_rule);
    // Use no proxy or otherwise this test will fail on a machine that has a
    // proxy configured.
    command_line->AppendSwitch(switches::kNoProxyServer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostRulesTest);
};

IN_PROC_BROWSER_TEST_F(HostRulesTest, TestMap) {
  // Go to the empty page using www.google.com as the host.
  GURL local_url = test_server()->GetURL("files/empty.html");
  GURL test_url(std::string("http://www.google.com") + local_url.path());
  ui_test_utils::NavigateToURL(browser(), test_url);

  std::string html;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      chrome::GetActiveWebContents(browser()),
      "window.domAutomationController.send(document.body.outerHTML);",
      &html));

  EXPECT_STREQ("<body></body>", html.c_str());
}

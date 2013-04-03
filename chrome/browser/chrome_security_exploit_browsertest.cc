// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

// The goal of these tests is to "simulate" exploited renderer processes, which
// can send arbitrary IPC messages and confuse browser process internal state,
// leading to security bugs. We are trying to verify that the browser doesn't
// perform any dangerous operations in such cases.
// This is similar to the security_exploit_browsertest.cc tests, but also
// includes chrome/ layer concepts such as extensions.
class ChromeSecurityExploitBrowserTest : public InProcessBrowserTest {
 public:
  ChromeSecurityExploitBrowserTest() {}
  virtual ~ChromeSecurityExploitBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    ASSERT_TRUE(test_server()->Start());
    net::TestServer https_server(
        net::TestServer::TYPE_HTTPS,
        net::TestServer::kLocalhost,
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    ASSERT_TRUE(https_server.Start());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");

    // Since we assume exploited renderer process, it can bypass the same origin
    // policy at will. Simulate that by passing the disable-web-security flag.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSecurityExploitBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromeSecurityExploitBrowserTest,
                       ChromeExtensionResources) {
  // Load a page that requests a chrome-extension:// image through XHR. We
  // expect this load to fail, as it is an illegal request.
  GURL foo("http://foo.com/files/chrome_extension_resource.html");

  content::DOMMessageQueue msg_queue;

  ui_test_utils::NavigateToURL(browser(), foo);

  std::string status;
  std::string expected_status("0");
  EXPECT_TRUE(msg_queue.WaitForMessage(&status));
  EXPECT_STREQ(status.c_str(), expected_status.c_str());
}

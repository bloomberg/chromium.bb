// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

namespace content {

// The goal of these tests will be to "simulate" exploited renderer processes,
// which can send arbitrary IPC messages and confuse browser process internal
// state, leading to security bugs. We are trying to verify that the browser
// doesn't perform any dangerous operations in such cases.
class SecurityExploitBrowserTest : public ContentBrowserTest {
 public:
  SecurityExploitBrowserTest() {}
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ASSERT_TRUE(test_server()->Start());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");
  }
};

// Ensure that we kill the renderer process if we try to give it WebUI
// properties and it doesn't have enabled WebUI bindings.
IN_PROC_BROWSER_TEST_F(SecurityExploitBrowserTest, SetWebUIProperty) {
  GURL foo("http://foo.com/files/simple_page.html");

  NavigateToURL(shell(), foo);
  EXPECT_EQ(0,
      shell()->web_contents()->GetRenderViewHost()->GetEnabledBindings());

  content::WindowedNotificationObserver terminated(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
  shell()->web_contents()->GetRenderViewHost()->SetWebUIProperty(
      "toolkit", "views");
  terminated.Wait();
}

}

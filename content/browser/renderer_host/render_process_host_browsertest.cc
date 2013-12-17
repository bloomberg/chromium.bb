// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {
namespace {

int RenderProcessHostCount() {
  content::RenderProcessHost::iterator hosts =
      content::RenderProcessHost::AllHostsIterator();
  int count = 0;
  while (!hosts.IsAtEnd()) {
    if (hosts.GetCurrentValue()->HasConnection())
      count++;
    hosts.Advance();
  }
  return count;
}

class RenderProcessHostTest : public ContentBrowserTest {};

// Sometimes the renderer process's ShutdownRequest (corresponding to the
// ViewMsg_WasSwappedOut from a previous navigation) doesn't arrive until after
// the browser process decides to re-use the renderer for a new purpose.  This
// test makes sure the browser doesn't let the renderer die in that case.  See
// http://crbug.com/87176.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       ShutdownRequestFromActiveTabIgnored) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateToURL(shell(), test_url);
  RenderProcessHost* rph =
      shell()->web_contents()->GetRenderViewHost()->GetProcess();

  TestNotificationTracker termination_watcher;
  termination_watcher.ListenFor(NOTIFICATION_RENDERER_PROCESS_CLOSED,
                                Source<RenderProcessHost>(rph));
  ChildProcessHostMsg_ShutdownRequest msg;
  rph->OnMessageReceived(msg);

  // If the RPH sends a mistaken ChildProcessMsg_Shutdown, the renderer process
  // will take some time to die. Wait for a second tab to load in order to give
  // that time to happen.
  NavigateToURL(CreateBrowser(), test_url);

  EXPECT_EQ(0U, termination_watcher.size());
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest,
                       GuestsAreNotSuitableHosts) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateToURL(shell(), test_url);
  RenderProcessHost* rph =
      shell()->web_contents()->GetRenderViewHost()->GetProcess();
  // Make it believe it's a guest.
  reinterpret_cast<RenderProcessHostImpl*>(rph)->SetIsGuestForTesting(true);
  EXPECT_EQ(1, RenderProcessHostCount());

  // Navigate to a different page.
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  GURL another_url = embedded_test_server()->GetURL("/simple_page.html");
  another_url = another_url.ReplaceComponents(replace_host);
  NavigateToURL(CreateBrowser(), another_url);

  // Expect that we got another process (the guest renderer was not reused).
  EXPECT_EQ(2, RenderProcessHostCount());
}

}  // namespace
}  // namespace content

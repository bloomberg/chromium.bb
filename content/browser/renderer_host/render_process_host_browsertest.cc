// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {
namespace {

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

}  // namespace
}  // namespace content

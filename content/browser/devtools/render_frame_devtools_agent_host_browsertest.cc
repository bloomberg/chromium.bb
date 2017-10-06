// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/controllable_http_response.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class RenderFrameDevToolsAgentHostBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
  }
};

// This test checks which RenderFrameHostImpl the RenderFrameDevToolsAgentHost
// is tracking while a cross-site navigation is canceled after having reached
// the ReadyToCommit stage.
// See https://crbug.com/695203.
IN_PROC_BROWSER_TEST_F(RenderFrameDevToolsAgentHostBrowserTest,
                       CancelCrossOriginNavigationAfterReadyToCommit) {
  if (!IsBrowserSideNavigationEnabled())
    return;

  ControllableHttpResponse response_b(embedded_test_server(), "/response_b");
  ControllableHttpResponse response_c(embedded_test_server(), "/response_c");
  EXPECT_TRUE(embedded_test_server()->Start());

  // 1) Loads a document.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents_impl->GetFrameTree()->root();

  // 2) Open DevTools.
  scoped_refptr<DevToolsAgentHost> devtools_agent =
      DevToolsAgentHost::GetOrCreateFor(web_contents_impl);
  RenderFrameDevToolsAgentHost* rfh_devtools_agent =
      static_cast<RenderFrameDevToolsAgentHost*>(devtools_agent.get());

  // 3) Tries to navigate cross-site, but do not commit the navigation.
  //    Send only the headers such that it reaches the ReadyToCommit stage, but
  //    not further.

  // 3.a) Navigation: Start.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/response_b"));
  TestNavigationManager observer_b(shell()->web_contents(), url_b);
  shell()->LoadURL(url_b);
  EXPECT_TRUE(observer_b.WaitForRequestStart());

  RenderFrameHostImpl* current_rfh =
      root->render_manager()->current_frame_host();
  RenderFrameHostImpl* speculative_rfh_b =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(current_rfh);
  EXPECT_TRUE(speculative_rfh_b);
  EXPECT_EQ(current_rfh, rfh_devtools_agent->GetFrameHostForTesting());

  // 3.b) Navigation: ReadyToCommit.
  observer_b.ResumeNavigation();  // Send the request.
  response_b.WaitForRequest();
  response_b.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  EXPECT_TRUE(observer_b.WaitForResponse());  // Headers are received.
  observer_b.ResumeNavigation();  // ReadyToCommitNavigation is called.
  EXPECT_EQ(speculative_rfh_b, rfh_devtools_agent->GetFrameHostForTesting());

  // 4) Navigate elsewhere, it will cancel the previous navigation.

  // 4.a) Navigation: Start.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/response_c"));
  TestNavigationManager observer_c(shell()->web_contents(), url_c);
  shell()->LoadURL(url_c);
  EXPECT_TRUE(observer_c.WaitForRequestStart());
  RenderFrameHostImpl* speculative_rfh_c =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh_c);
  EXPECT_EQ(current_rfh, rfh_devtools_agent->GetFrameHostForTesting());

  // 4.b) Navigation: ReadyToCommit.
  observer_c.ResumeNavigation();  // Send the request.
  response_c.WaitForRequest();
  response_c.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  EXPECT_TRUE(observer_c.WaitForResponse());  // Headers are received.
  observer_c.ResumeNavigation();  // ReadyToCommitNavigation is called.
  EXPECT_EQ(speculative_rfh_c, rfh_devtools_agent->GetFrameHostForTesting());

  // 4.c) Navigation: Commit.
  response_c.Send("<html><body> response's body </body></html>");
  response_c.Done();
  observer_c.WaitForNavigationFinished();
  EXPECT_EQ(speculative_rfh_c, root->render_manager()->current_frame_host());
  EXPECT_EQ(speculative_rfh_c, rfh_devtools_agent->GetFrameHostForTesting());
}

}  // namespace content

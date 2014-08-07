// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class FrameTreeBrowserTest : public ContentBrowserTest {
 public:
  FrameTreeBrowserTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameTreeBrowserTest);
};

// Ensures FrameTree correctly reflects page structure during navigations.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = test_server()->GetURL("files/site_isolation/");
  GURL::Replacements replace_host;
  std::string host_str("A.com");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  // Load doc without iframes. Verify FrameTree just has root.
  // Frame tree:
  //   Site-A Root
  NavigateToURL(shell(), base_url.Resolve("blank.html"));
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();
  EXPECT_EQ(0U, root->child_count());

  // Add 2 same-site frames. Verify 3 nodes in tree with proper names.
  // Frame tree:
  //   Site-A Root -- Site-A frame1
  //              \-- Site-A frame2
  WindowedNotificationObserver observer1(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &shell()->web_contents()->GetController()));
  NavigateToURL(shell(), base_url.Resolve("frames-X-X.html"));
  observer1.Wait();
  ASSERT_EQ(2U, root->child_count());
  EXPECT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(0U, root->child_at(1)->child_count());
}

// TODO(ajwong): Talk with nasko and merge this functionality with
// FrameTreeShape.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape2) {
  ASSERT_TRUE(test_server()->Start());
  NavigateToURL(shell(),
                test_server()->GetURL("files/frame_tree/top.html"));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_STREQ("1-1-name", root->child_at(0)->frame_name().c_str());

  // Verify the deepest node exists and has the right name.
  ASSERT_EQ(2UL, root->child_at(2)->child_count());
  EXPECT_EQ(1UL, root->child_at(2)->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(1)->child_at(0)->child_count());
  EXPECT_STREQ("3-1-name",
      root->child_at(2)->child_at(1)->child_at(0)->frame_name().c_str());

  // Navigate to about:blank, which should leave only the root node of the frame
  // tree in the browser process.
  NavigateToURL(shell(), test_server()->GetURL("files/title1.html"));

  root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeAfterCrash) {
  ASSERT_TRUE(test_server()->Start());
  NavigateToURL(shell(),
                test_server()->GetURL("files/frame_tree/top.html"));

  // Crash the renderer so that it doesn't send any FrameDetached messages.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  NavigateToURL(shell(), GURL(kChromeUICrashURL));
  crash_observer.Wait();

  // The frame tree should be cleared.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());

  // Navigate to a new URL.
  GURL url(test_server()->GetURL("files/title1.html"));
  NavigateToURL(shell(), url);
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(url, root->current_url());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateWithLeftoverFrames) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = test_server()->GetURL("files/site_isolation/");
  GURL::Replacements replace_host;
  std::string host_str("A.com");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  NavigateToURL(shell(),
                test_server()->GetURL("files/frame_tree/top.html"));

  // Hang the renderer so that it doesn't send any FrameDetached messages.
  // (This navigation will never complete, so don't wait for it.)
  shell()->LoadURL(GURL(kChromeUIHangURL));

  // Check that the frame tree still has children.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(3UL, root->child_count());

  // Navigate to a new URL.  We use LoadURL because NavigateToURL will try to
  // wait for the previous navigation to stop.
  TestNavigationObserver tab_observer(wc, 1);
  shell()->LoadURL(base_url.Resolve("blank.html"));
  tab_observer.Wait();

  // The frame tree should now be cleared.
  EXPECT_EQ(0UL, root->child_count());
}

class CrossProcessFrameTreeBrowserTest : public ContentBrowserTest {
 public:
  CrossProcessFrameTreeBrowserTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSitePerProcess);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossProcessFrameTreeBrowserTest);
};

// Ensure that we can complete a cross-process subframe navigation.
#if defined(OS_ANDROID)
#define MAYBE_CreateCrossProcessSubframeProxies DISABLED_CreateCrossProcessSubframeProxies
#else
#define MAYBE_CreateCrossProcessSubframeProxies CreateCrossProcessSubframeProxies
#endif
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       MAYBE_CreateCrossProcessSubframeProxies) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // Load same-site page into iframe.
  GURL http_url(test_server()->GetURL("files/title1.html"));
  NavigateFrameToURL(root->child_at(0), http_url);

  // These must stay in scope with replace_host.
  GURL::Replacements replace_host;
  std::string foo_com("foo.com");

  // Load cross-site page into iframe.
  GURL cross_site_url(test_server()->GetURL("files/title2.html"));
  replace_host.SetHostStr(foo_com);
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  RenderViewHost* rvh = child->current_frame_host()->render_view_host();
  RenderProcessHost* rph = child->current_frame_host()->GetProcess();

  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(), rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);
  EXPECT_NE(shell()->web_contents()->GetRenderProcessHost(), rph);

  // Ensure that the root node has a proxy for the child node's SiteInstance.
  EXPECT_TRUE(root->render_manager()->proxy_hosts_[site_instance->GetId()]);

  // Also ensure that the child has a proxy for the root node's SiteInstance.
  EXPECT_TRUE(child->render_manager()->proxy_hosts_[
              root->current_frame_host()->GetSiteInstance()->GetId()]);
}

}  // namespace content

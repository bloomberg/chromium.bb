// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace content {

class IsolatedOriginTest : public ContentBrowserTest {
 public:
  IsolatedOriginTest() {}
  ~IsolatedOriginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::string origin_list =
        embedded_test_server()->GetURL("isolated.foo.com", "/").spec() + "," +
        embedded_test_server()->GetURL("isolated.bar.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};

// Check that navigating a main frame from an non-isolated origin to an
// isolated origin and vice versa swaps processes and uses a new SiteInstance,
// both for browser-initiated and renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, MainFrameNavigation) {
  GURL unisolated_url(
      embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), unisolated_url));

  // Open a same-site popup to keep the www.foo.com process alive.
  Shell* popup = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  SiteInstance* unisolated_instance =
      popup->web_contents()->GetMainFrame()->GetSiteInstance();
  RenderProcessHost* unisolated_process =
      popup->web_contents()->GetMainFrame()->GetProcess();

  // Perform a browser-initiated navigation to an isolated origin and ensure
  // that this ends up in a new process and SiteInstance for isolated.foo.com.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  scoped_refptr<SiteInstance> isolated_instance =
      web_contents()->GetSiteInstance();
  EXPECT_NE(isolated_instance, unisolated_instance);
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(), unisolated_process);

  // The site URL for isolated.foo.com should be the full origin rather than
  // scheme and eTLD+1.
  EXPECT_EQ(isolated_url.GetOrigin(), isolated_instance->GetSiteURL());

  // Now perform a renderer-initiated navigation to an unisolated origin,
  // www.foo.com. This should end up in the |popup|'s process.
  {
    TestNavigationObserver observer(web_contents());
    EXPECT_TRUE(ExecuteScript(
        web_contents(), "location.href = '" + unisolated_url.spec() + "'"));
    observer.Wait();
  }

  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Go to isolated.foo.com again, this time with a renderer-initiated
  // navigation from the unisolated www.foo.com.
  {
    TestNavigationObserver observer(web_contents());
    EXPECT_TRUE(ExecuteScript(web_contents(),
                              "location.href = '" + isolated_url.spec() + "'"));
    observer.Wait();
  }

  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Go back to www.foo.com: this should end up in the unisolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Go back again.  This should go to isolated.foo.com in an isolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Do a renderer-initiated navigation from isolated.foo.com to another
  // isolated origin and ensure there is a different isolated process.
  GURL second_isolated_url(
      embedded_test_server()->GetURL("isolated.bar.com", "/title3.html"));
  {
    TestNavigationObserver observer(web_contents());
    EXPECT_TRUE(
        ExecuteScript(web_contents(),
                      "location.href = '" + second_isolated_url.spec() + "'"));
    observer.Wait();
  }

  EXPECT_EQ(second_isolated_url.GetOrigin(),
            web_contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_instance, web_contents()->GetSiteInstance());
}

// Check that opening a popup for an isolated origin puts it into a new process
// and its own SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, Popup) {
  GURL unisolated_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), unisolated_url));

  // Open a popup to a URL with an isolated origin and ensure that there was a
  // process swap.
  Shell* popup = OpenPopup(shell(), isolated_url, "foo");

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            popup->web_contents()->GetSiteInstance());

  // The popup's site URL should match the full isolated origin.
  EXPECT_EQ(isolated_url.GetOrigin(),
            popup->web_contents()->GetSiteInstance()->GetSiteURL());

  // Now open a second popup from an isolated origin to a URL with an
  // unisolated origin and ensure that there was another process swap.
  Shell* popup2 = OpenPopup(popup, unisolated_url, "bar");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            popup2->web_contents()->GetSiteInstance());
  EXPECT_NE(popup->web_contents()->GetSiteInstance(),
            popup2->web_contents()->GetSiteInstance());
}

// Check that navigating a subframe to an isolated origin puts the subframe
// into an OOPIF and its own SiteInstance.  Also check that the isolated
// frame's subframes also end up in correct SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, Subframe) {
  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(child->current_url(), isolated_url);

  // Verify that the child frame is an OOPIF with a different SiteInstance.
  EXPECT_NE(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(isolated_url.GetOrigin(),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // Verify that the isolated frame's subframe (which starts out at a relative
  // path) is kept in the isolated parent's SiteInstance.
  FrameTreeNode* grandchild = child->child_at(0);
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());

  // Navigating the grandchild to www.foo.com should put it into the top
  // frame's SiteInstance.
  GURL non_isolated_url(
      embedded_test_server()->GetURL("www.foo.com", "/title3.html"));
  TestFrameNavigationObserver observer(grandchild);
  EXPECT_TRUE(ExecuteScript(
      grandchild, "location.href = '" + non_isolated_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(non_isolated_url, grandchild->current_url());

  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
}

// Check that when an non-isolated origin foo.com embeds a subframe from an
// isolated origin, which then navigates to a non-isolated origin bar.com,
// bar.com goes back to the main frame's SiteInstance.  See
// https://crbug.com/711006.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       NoOOPIFWhenIsolatedOriginNavigatesToNonIsolatedOrigin) {
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));

  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(isolated_url, child->current_url());

  // Verify that the child frame is an OOPIF with a different SiteInstance.
  EXPECT_NE(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(isolated_url.GetOrigin(),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // Navigate the child frame cross-site, but to a non-isolated origin. When
  // not in --site-per-process, this should bring the subframe back into the
  // main frame's SiteInstance.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_FALSE(policy->IsIsolatedOrigin(url::Origin(bar_url)));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()->IsCrossProcessSubframe());
}

// Check that isolated origins can access cookies.  This requires cookie checks
// on the IO thread to be aware of isolated origins.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, Cookies) {
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  EXPECT_TRUE(ExecuteScript(web_contents(), "document.cookie = 'foo=bar';"));

  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(), "window.domAutomationController.send(document.cookie);",
      &cookie));
  EXPECT_EQ("foo=bar", cookie);
}

}  // namespace content

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
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
  EXPECT_FALSE(policy->IsIsolatedOrigin(url::Origin::Create(bar_url)));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()->IsCrossProcessSubframe());
}

// Check that a new isolated origin subframe will attempt to reuse an existing
// process for that isolated origin, even across BrowsingInstances.  Also check
// that main frame navigations to an isolated origin keep using the default
// process model and do not reuse existing processes.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, SubframeReusesExistingProcess) {
  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Open an unrelated tab in a separate BrowsingInstance, and navigate it to
  // to an isolated origin.  This SiteInstance should have a default process
  // reuse policy - only subframes attempt process reuse.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, isolated_url));
  scoped_refptr<SiteInstanceImpl> second_shell_instance =
      static_cast<SiteInstanceImpl*>(
          second_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      root->current_frame_host()->GetSiteInstance()));
  RenderProcessHost* isolated_process = second_shell_instance->GetProcess();
  EXPECT_EQ(SiteInstanceImpl::ProcessReusePolicy::DEFAULT,
            second_shell_instance->process_reuse_policy());

  // Now navigate the first tab's subframe to an isolated origin.  See that it
  // reuses the existing |isolated_process|.
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(isolated_url, child->current_url());
  EXPECT_EQ(isolated_process, child->current_frame_host()->GetProcess());
  EXPECT_EQ(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE,
      child->current_frame_host()->GetSiteInstance()->process_reuse_policy());

  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(isolated_url.GetOrigin(),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // The subframe's SiteInstance should still be different from second_shell's
  // SiteInstance, and they should be in separate BrowsingInstances.
  EXPECT_NE(second_shell_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      child->current_frame_host()->GetSiteInstance()));

  // Navigate the second tab to a normal URL with a same-site subframe.  This
  // leaves only the first tab's subframe in the isolated origin process.
  EXPECT_TRUE(NavigateToURL(second_shell, top_url));
  EXPECT_NE(isolated_process,
            second_shell->web_contents()->GetMainFrame()->GetProcess());

  // Navigate the second tab's subframe to an isolated origin, and check that
  // this new subframe reuses the isolated process of the subframe in the first
  // tab, even though the two are in separate BrowsingInstances.
  NavigateIframeToURL(second_shell->web_contents(), "test_iframe",
                      isolated_url);
  FrameTreeNode* second_subframe =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetFrameTree()
          ->root()
          ->child_at(0);
  EXPECT_EQ(isolated_process,
            second_subframe->current_frame_host()->GetProcess());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            second_subframe->current_frame_host()->GetSiteInstance());

  // Open a third, unrelated tab, navigate it to an isolated origin, and check
  // that its main frame doesn't share a process with the existing isolated
  // subframes.
  Shell* third_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(third_shell, isolated_url));
  SiteInstanceImpl* third_shell_instance = static_cast<SiteInstanceImpl*>(
      third_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            second_subframe->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance->GetProcess(), isolated_process);
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

// Check that isolated origins won't be placed into processes for other sites
// when over the process limit.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, ProcessLimit) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Navigate to an unisolated foo.com URL with an iframe.
  GURL foo_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderProcessHost* foo_process = root->current_frame_host()->GetProcess();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe to an isolated origin.
  GURL isolated_foo_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_foo_url);

  // Ensure that the subframe was rendered in a new process.
  EXPECT_NE(child->current_frame_host()->GetProcess(), foo_process);

  // Sanity-check IsSuitableHost values for the current processes.
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  auto is_suitable_host = [browser_context](RenderProcessHost* process,
                                            GURL url) {
    return RenderProcessHostImpl::IsSuitableHost(
        process, browser_context,
        SiteInstance::GetSiteForURL(browser_context, url));
  };
  EXPECT_TRUE(is_suitable_host(foo_process, foo_url));
  EXPECT_FALSE(is_suitable_host(foo_process, isolated_foo_url));
  EXPECT_TRUE(is_suitable_host(child->current_frame_host()->GetProcess(),
                               isolated_foo_url));
  EXPECT_FALSE(
      is_suitable_host(child->current_frame_host()->GetProcess(), foo_url));

  // Open a new, unrelated tab and navigate it to isolated.foo.com.  This
  // should use a new, unrelated SiteInstance that reuses the existing isolated
  // origin process from first tab's subframe.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, isolated_foo_url));
  scoped_refptr<SiteInstance> isolated_foo_instance(
      new_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  RenderProcessHost* isolated_foo_process = isolated_foo_instance->GetProcess();
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            isolated_foo_instance);
  EXPECT_FALSE(isolated_foo_instance->IsRelatedSiteInstance(
      child->current_frame_host()->GetSiteInstance()));
  // TODO(alexmos): with --site-per-process, this won't currently reuse the
  // subframe process, because the new SiteInstance will initialize its
  // process while it still has no site (during CreateBrowser()), and since
  // dedicated processes can't currently be reused for a SiteInstance with no
  // site, this creates a new process.  The subsequent navigation to
  // |isolated_foo_url| stays in that new process without consulting whether it
  // can now reuse a different process.  This should be fixed; see
  // https://crbug.com/513036.   Without --site-per-process, this works because
  // the site-less SiteInstance is allowed to reuse the first tab's foo.com
  // process (which isn't dedicated), and then it swaps to the isolated.foo.com
  // process during navigation.
  if (!AreAllSitesIsolatedForTesting())
    EXPECT_EQ(child->current_frame_host()->GetProcess(), isolated_foo_process);

  // Navigate iframe on the first tab to a non-isolated site.  This should swap
  // processes so that it does not reuse the isolated origin's process.
  RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
  NavigateIframeToURL(
      web_contents(), "test_iframe",
      embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  EXPECT_EQ(foo_process, child->current_frame_host()->GetProcess());
  EXPECT_NE(isolated_foo_process, child->current_frame_host()->GetProcess());
  deleted_observer.WaitUntilDeleted();

  // Navigate iframe back to isolated origin.  See that it reuses the
  // |new_shell| process.
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_foo_url);
  EXPECT_NE(foo_process, child->current_frame_host()->GetProcess());
  EXPECT_EQ(isolated_foo_process, child->current_frame_host()->GetProcess());

  // Navigate iframe to a different isolated origin.  Ensure that this creates
  // a third process.
  GURL isolated_bar_url(
      embedded_test_server()->GetURL("isolated.bar.com", "/title3.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_bar_url);
  RenderProcessHost* isolated_bar_process =
      child->current_frame_host()->GetProcess();
  EXPECT_NE(foo_process, isolated_bar_process);
  EXPECT_NE(isolated_foo_process, isolated_bar_process);

  // The new process should only be suitable to host isolated.bar.com, not
  // regular web URLs or other isolated origins.
  EXPECT_TRUE(is_suitable_host(isolated_bar_process, isolated_bar_url));
  EXPECT_FALSE(is_suitable_host(isolated_bar_process, foo_url));
  EXPECT_FALSE(is_suitable_host(isolated_bar_process, isolated_foo_url));

  // Navigate second tab (currently at isolated.foo.com) to the
  // second isolated origin, and see that it switches processes.
  EXPECT_TRUE(NavigateToURL(new_shell, isolated_bar_url));
  EXPECT_NE(foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_NE(isolated_foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_EQ(isolated_bar_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());

  // Navigate second tab to a non-isolated URL and see that it goes back into
  // the www.foo.com process, and that it does not share processes with any
  // isolated origins.
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  EXPECT_EQ(foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_NE(isolated_foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_NE(isolated_bar_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// Verify that a navigation to an non-isolated origin does not reuse a process
// from a pending navigation to an isolated origin.  See
// https://crbug.com/738634.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       ProcessReuseWithResponseStartedFromIsolatedOrigin) {
  // This test requires PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start, but don't commit a navigation to an unisolated foo.com URL.
  GURL slow_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  NavigationController::LoadURLParams load_params(slow_url);
  TestNavigationManager foo_delayer(shell()->web_contents(), slow_url);
  shell()->web_contents()->GetController().LoadURL(
      slow_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(foo_delayer.WaitForRequestStart());

  // Open a new, unrelated tab and navigate it to isolated.foo.com.
  Shell* new_shell = CreateBrowser();
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  TestNavigationManager isolated_delayer(new_shell->web_contents(),
                                         isolated_url);
  new_shell->web_contents()->GetController().LoadURL(
      isolated_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // Wait for response from the isolated origin.  After this returns,
  // PlzNavigate has made the final pick for the process to use for this
  // navigation as part of NavigationRequest::OnResponseStarted.
  EXPECT_TRUE(isolated_delayer.WaitForResponse());

  // Now, proceed with the response and commit the non-isolated URL.  This
  // should notice that the process that was picked for this navigation is not
  // suitable anymore, as it should have been locked to isolated.foo.com.
  foo_delayer.WaitForNavigationFinished();

  // Commit the isolated origin.
  isolated_delayer.WaitForNavigationFinished();

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// Verify that a navigation to an unisolated origin cannot reuse a process from
// a pending navigation to an isolated origin.  Similar to
// ProcessReuseWithResponseStartedFromIsolatedOrigin, but here the non-isolated
// URL is the first to reach OnResponseStarted, which should mark the process
// as "used", so that the isolated origin can't reuse it. See
// https://crbug.com/738634.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       ProcessReuseWithResponseStartedFromUnisolatedOrigin) {
  // This test requires PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start a navigation to an unisolated foo.com URL.
  GURL slow_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  NavigationController::LoadURLParams load_params(slow_url);
  TestNavigationManager foo_delayer(shell()->web_contents(), slow_url);
  shell()->web_contents()->GetController().LoadURL(
      slow_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // Wait for response for foo.com.  After this returns,
  // PlzNavigate should have made the final pick for the process to use for
  // foo.com, so this should mark the process as "used" and ineligible for
  // reuse by isolated.foo.com below.
  EXPECT_TRUE(foo_delayer.WaitForResponse());

  // Open a new, unrelated tab, navigate it to isolated.foo.com, and wait for
  // the navigation to fully load.
  Shell* new_shell = CreateBrowser();
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, isolated_url));

  // Finish loading the foo.com URL.
  foo_delayer.WaitForNavigationFinished();

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// Check that subdomains on an isolated origin (e.g., bar.isolated.foo.com)
// also end up in the isolated origin's SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, IsolatedOriginWithSubdomain) {
  // Start on a page with an isolated origin with a same-site iframe.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstance> isolated_instance =
      web_contents()->GetSiteInstance();

  // Navigate iframe to the isolated origin's subdomain.
  GURL isolated_subdomain_url(
      embedded_test_server()->GetURL("bar.isolated.foo.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_subdomain_url);
  EXPECT_EQ(child->current_url(), isolated_subdomain_url);

  EXPECT_EQ(isolated_instance, child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(isolated_url.GetOrigin(),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // Now try navigating the main frame (renderer-initiated) to the isolated
  // origin's subdomain.  This should not swap processes.
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecuteScript(web_contents(),
                    "location.href = '" + isolated_subdomain_url.spec() + "'"));
  observer.Wait();
  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
}

// This class allows intercepting the OpenLocalStorage method and changing
// the parameters to the real implementation of it.
class StoragePartitonInterceptor
    : public mojom::StoragePartitionServiceInterceptorForTesting,
      public RenderProcessHostObserver {
 public:
  StoragePartitonInterceptor(RenderProcessHostImpl* rph,
                             mojom::StoragePartitionServiceRequest request) {
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(rph->GetStoragePartition());

    // Bind the real StoragePartitionService implementation.
    mojo::BindingId binding_id =
        storage_partition->Bind(rph->GetID(), std::move(request));

    // Now replace it with this object and keep a pointer to the real
    // implementation.
    storage_partition_service_ =
        storage_partition->bindings_for_testing().SwapImplForTesting(binding_id,
                                                                     this);

    // Register the |this| as a RenderProcessHostObserver, so it can be
    // correctly cleaned up when the process exits.
    rph->AddObserver(this);
  }

  // Ensure this object is cleaned up when the process goes away, since it
  // is not owned by anyone else.
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override {
    host->RemoveObserver(this);
    delete this;
  }

  // Allow all methods that aren't explicitly overriden to pass through
  // unmodified.
  mojom::StoragePartitionService* GetForwardingInterface() override {
    return storage_partition_service_;
  }

  // Override this method to allow changing the origin. It simulates a
  // renderer process sending incorrect data to the browser process, so
  // security checks can be tested.
  void OpenLocalStorage(const url::Origin& origin,
                        mojom::LevelDBWrapperRequest request) override {
    url::Origin mismatched_origin =
        url::Origin::Create(GURL("http://abc.foo.com"));
    GetForwardingInterface()->OpenLocalStorage(mismatched_origin,
                                               std::move(request));
  }

 private:
  // Keep a pointer to the original implementation of the service, so all
  // calls can be forwarded to it.
  mojom::StoragePartitionService* storage_partition_service_;
};

void CreateTestStoragePartitionService(
    RenderProcessHostImpl* rph,
    mojom::StoragePartitionServiceRequest request) {
  // This object will register as RenderProcessHostObserver, so it will
  // clean itself automatically on process exit.
  new StoragePartitonInterceptor(rph, std::move(request));
}

// Verify that an isolated renderer process cannot read localStorage of an
// origin outside of its isolated site.
// TODO(nasko): Write a test to verify the opposite - any non-isolated renderer
// process cannot access data of an isolated site.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, LocalStorageOriginEnforcement) {
  RenderProcessHostImpl::SetCreateStoragePartitionServiceFunction(
      CreateTestStoragePartitionService);

  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  content::RenderProcessHostWatcher crash_observer(
      shell()->web_contents()->GetMainFrame()->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  // Use ignore_result here, since on Android the renderer process is
  // terminated, but ExecuteScript still returns true. It properly returns
  // false on all other platforms.
  ignore_result(ExecuteScript(shell()->web_contents()->GetMainFrame(),
                              "localStorage.length;"));
  crash_observer.Wait();
}

}  // namespace content

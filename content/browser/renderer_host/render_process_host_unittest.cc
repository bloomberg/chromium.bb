// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

class RenderProcessHostUnitTest : public RenderViewHostImplTestHarness {};

// Tests that guest RenderProcessHosts are not considered suitable hosts when
// searching for RenderProcessHost.
TEST_F(RenderProcessHostUnitTest, GuestsAreNotSuitableHosts) {
  GURL test_url("http://foo.com");

  MockRenderProcessHost guest_host(browser_context());
  guest_host.set_is_for_guests_only(true);

  EXPECT_FALSE(RenderProcessHostImpl::IsSuitableHost(
      &guest_host, browser_context(), test_url));
  EXPECT_TRUE(RenderProcessHostImpl::IsSuitableHost(
      process(), browser_context(), test_url));
  EXPECT_EQ(
      process(),
      RenderProcessHost::GetExistingProcessHost(browser_context(), test_url));
}

#if !defined(OS_ANDROID)
TEST_F(RenderProcessHostUnitTest, RendererProcessLimit) {
  // This test shouldn't run with --site-per-process mode, which prohibits
  // the renderer process reuse this test explicitly exercises.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Disable any overrides.
  RenderProcessHostImpl::SetMaxRendererProcessCount(0);

  // Verify that the limit is between 1 and kMaxRendererProcessCount.
  EXPECT_GT(RenderProcessHostImpl::GetMaxRendererProcessCount(), 0u);
  EXPECT_LE(RenderProcessHostImpl::GetMaxRendererProcessCount(),
      kMaxRendererProcessCount);

  // Add dummy process hosts to saturate the limit.
  ASSERT_NE(0u, kMaxRendererProcessCount);
  std::vector<std::unique_ptr<MockRenderProcessHost>> hosts;
  for (size_t i = 0; i < kMaxRendererProcessCount; ++i) {
    hosts.push_back(base::MakeUnique<MockRenderProcessHost>(browser_context()));
  }

  // Verify that the renderer sharing will happen.
  GURL test_url("http://foo.com");
  EXPECT_TRUE(RenderProcessHostImpl::ShouldTryToUseExistingProcessHost(
        browser_context(), test_url));
}
#endif

#if defined(OS_ANDROID)
TEST_F(RenderProcessHostUnitTest, NoRendererProcessLimitOnAndroid) {
  // Disable any overrides.
  RenderProcessHostImpl::SetMaxRendererProcessCount(0);

  // Add a few dummy process hosts.
  ASSERT_NE(0u, kMaxRendererProcessCount);
  std::vector<std::unique_ptr<MockRenderProcessHost>> hosts;
  for (size_t i = 0; i < kMaxRendererProcessCount; ++i) {
    hosts.push_back(base::MakeUnique<MockRenderProcessHost>(browser_context()));
  }

  // Verify that the renderer sharing still won't happen.
  GURL test_url("http://foo.com");
  EXPECT_FALSE(RenderProcessHostImpl::ShouldTryToUseExistingProcessHost(
        browser_context(), test_url));
}
#endif

// Tests that RenderProcessHost reuse considers committed sites correctly.
TEST_F(RenderProcessHostUnitTest, ReuseCommittedSite) {
  const GURL kUrl1("http://foo.com");
  const GURL kUrl2("http://bar.com");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Have the main frame navigate to the first url. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy should now return the
  // process of the main RFH.
  NavigateAndCommit(kUrl1);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Navigate away. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should again return a new process.
  NavigateAndCommit(kUrl2);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Now add a subframe that navigates to kUrl1. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy for kUrl1 should now
  // return the process of the subframe RFH.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(), blink::WebTreeScopeType::kDocument,
      std::string(), unique_name, blink::WebSandboxFlags::kNone,
      ParsedFeaturePolicyHeader(), FrameOwnerProperties());
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.frame_unique_name = unique_name;
    params.did_create_new_entry = false;
    params.url = kUrl1;
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    params.should_update_history = false;
    params.gesture = NavigationGestureUser;
    params.method = "GET";
    params.page_state = PageState::CreateFromURL(kUrl1);
    subframe->SendRendererInitiatedNavigationRequest(kUrl1, false);
    subframe->PrepareForCommit();
    subframe->SendNavigateWithParams(&params);
  }
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(subframe->GetProcess(), site_instance->GetProcess());
}

// Tests that RenderProcessHost will not consider reusing a process that has
// committed an error page.
TEST_F(RenderProcessHostUnitTest, DoNotReuseError) {
  const GURL kUrl1("http://foo.com");
  const GURL kUrl2("http://bar.com");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Have the main frame navigate to the first url. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy should now return the
  // process of the main RFH.
  NavigateAndCommit(kUrl1);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Navigate away. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should again return a new process.
  NavigateAndCommit(kUrl2);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Navigate back and simulate an error. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  web_contents()->GetController().GoBack();
  TestRenderFrameHost* pending_rfh = contents()->GetPendingMainFrame();
  if (!IsBrowserSideNavigationEnabled())
    pending_rfh->SimulateNavigationStart(kUrl1);
  pending_rfh->SimulateNavigationError(kUrl1, net::ERR_TIMED_OUT);
  pending_rfh->SimulateNavigationErrorPageCommit();
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
}

// Tests that RenderProcessHost reuse considers navigations correctly.
TEST_F(RenderProcessHostUnitTest, ReuseNavigationProcess) {
  // This is only applicable to PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  const GURL kUrl1("http://foo.com");
  const GURL kUrl2("http://bar.com");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Now Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current process.
  main_test_rfh()->SimulateNavigationStart(kUrl1);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Finish the navigation and start a new cross-site one. Getting
  // RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy should
  // return the process of the speculative RenderFrameHost.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, kUrl1);
  contents()->GetController().LoadURL(kUrl2, Referrer(),
                                      ui::PAGE_TRANSITION_TYPED, std::string());
  main_test_rfh()->SendBeforeUnloadACK(true);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl2);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(contents()->GetPendingMainFrame()->GetProcess(),
            site_instance->GetProcess());

  // Remember the process id and cancel the navigation. Getting
  // RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy should
  // no longer return the process of the speculative RenderFrameHost.
  int speculative_process_host_id =
      contents()->GetPendingMainFrame()->GetProcess()->GetID();
  contents()->GetPendingMainFrame()->SimulateNavigationError(kUrl2,
                                                             net::ERR_ABORTED);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl2);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(speculative_process_host_id, site_instance->GetProcess()->GetID());
}

// Tests that RenderProcessHost reuse considers navigations correctly during
// redirects.
TEST_F(RenderProcessHostUnitTest, ReuseNavigationProcessRedirects) {
  // This is only applicable to PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  const GURL kUrl("http://foo.com");
  const GURL kRedirectUrl1("http://foo.com/redirect");
  const GURL kRedirectUrl2("http://bar.com");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Now getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current process.
  main_test_rfh()->SimulateNavigationStart(kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Simulate a same-site redirect. Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current process.
  main_test_rfh()->SimulateRedirect(kRedirectUrl1);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Simulate a cross-site redirect. Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should not return the current
  // process, whether for the old site or the new site.
  main_test_rfh()->SimulateRedirect(kRedirectUrl2);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kRedirectUrl2);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Once the navigation is ready to commit, Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should not return the current
  // process for the final site, but not the initial one.
  main_test_rfh()->PrepareForCommit();
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kRedirectUrl2);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());
}

class EffectiveURLContentBrowserClient : public ContentBrowserClient {
 public:
  EffectiveURLContentBrowserClient(const GURL& url_to_modify,
                                   const GURL& url_to_return)
      : url_to_modify_(url_to_modify), url_to_return_(url_to_return) {}
  ~EffectiveURLContentBrowserClient() override {}

 private:
  GURL GetEffectiveURL(BrowserContext* browser_context,
                       const GURL& url) override {
    if (url == url_to_modify_)
      return url_to_return_;
    return url;
  }

  GURL url_to_modify_;
  GURL url_to_return_;
};

// Tests that RenderProcessHost reuse works correctly even if the site URL of a
// URL changes.
TEST_F(RenderProcessHostUnitTest, ReuseSiteURLChanges) {
  const GURL kUrl("http://foo.com");
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Have the main frame navigate to the first url. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy should now return the
  // process of the main RFH.
  NavigateAndCommit(kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Install the custom ContentBrowserClient. Site URLs are now modified.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as the RFH is
  // registered with the normal site URL.
  EffectiveURLContentBrowserClient modified_client(kUrl, kModifiedSiteUrl);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it is now registered with the modified site URL.
  contents()->GetController().Reload(ReloadType::NORMAL, false);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Remove the custom ContentBrowserClient. Site URLs are back to normal.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as it is registered
  // with the modified site URL.
  SetBrowserClientForTesting(regular_client);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it is now registered with the regular site URL.
  contents()->GetController().Reload(ReloadType::NORMAL, false);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());
}

// Tests that RenderProcessHost reuse works correctly even if the site URL of a
// URL we're navigating to changes.
TEST_F(RenderProcessHostUnitTest, ReuseExpectedSiteURLChanges) {
  // This is only applicable to PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  const GURL kUrl("http://foo.com");
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH.
  main_test_rfh()->SimulateNavigationStart(kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Install the custom ContentBrowserClient. Site URLs are now modified.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as the RFH is
  // registered with the normal site URL.
  EffectiveURLContentBrowserClient modified_client(kUrl, kModifiedSiteUrl);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Have the navigation commit. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it was registered with the modified site URL at commit time.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the process of the
  // main RFH.
  contents()->GetController().Reload(ReloadType::NORMAL, false);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Remove the custom ContentBrowserClient. Site URLs are back to normal.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as it is registered
  // with the modified site URL.
  SetBrowserClientForTesting(regular_client);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Finish the reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it was registered with the regular site URL when it committed.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());
}

}  // namespace content

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/frame_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"

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
      std::string(), unique_name, base::UnguessableToken::Create(),
      FramePolicy(), FrameOwnerProperties());
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  subframe = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, subframe));
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(subframe->GetProcess(), site_instance->GetProcess());
}

// Check that only new processes that haven't yet hosted any web content are
// allowed to be reused to host a site requiring a dedicated process.
TEST_F(RenderProcessHostUnitTest, IsUnused) {
  const GURL kUrl1("http://foo.com");

  // A process for a SiteInstance that has no site should be able to host any
  // site that requires a dedicated process.
  EXPECT_FALSE(main_test_rfh()->GetSiteInstance()->HasSite());
  EXPECT_TRUE(main_test_rfh()->GetProcess()->IsUnused());
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::Create(browser_context());
    EXPECT_FALSE(site_instance->HasSite());
    EXPECT_TRUE(site_instance->GetProcess()->IsUnused());
  }

  // Navigation should mark the process as unable to become a dedicated process
  // for arbitrary sites.
  NavigateAndCommit(kUrl1);
  EXPECT_FALSE(main_test_rfh()->GetProcess()->IsUnused());

  // A process for a SiteInstance with a preassigned site should be considered
  // "used" from the point the process is created via GetProcess().
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
    EXPECT_FALSE(site_instance->GetProcess()->IsUnused());
  }
}

TEST_F(RenderProcessHostUnitTest, ReuseUnmatchedServiceWorkerProcess) {
  const GURL kUrl("https://foo.com");

  // Gets a RenderProcessHost for an unmatched service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance1->set_is_for_service_worker();
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();

  // Getting a RenderProcessHost for a service worker with DEFAULT reuse policy
  // should not reuse the existing service worker's process. We create this
  // second service worker to test the "find the newest process" logic later.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance2->set_is_for_service_worker();
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_NE(sw_host1, sw_host2);

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host2).
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  EXPECT_EQ(sw_host2, site_instance1->GetProcess());

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host1). sw_host2
  // is no longer unmatched, so sw_host1 is now the newest (and only) process
  // with a corresponding unmatched service worker.
  scoped_refptr<SiteInstanceImpl> site_instance2 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  EXPECT_EQ(sw_host1, site_instance2->GetProcess());

  // Getting a RenderProcessHost for a navigation should return a new process
  // because there is no unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> site_instance3 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  EXPECT_NE(sw_host1, site_instance3->GetProcess());
  EXPECT_NE(sw_host2, site_instance3->GetProcess());
}

TEST_F(RenderProcessHostUnitTest, ReuseServiceWorkerProcessForServiceWorker) {
  const GURL kUrl("https://foo.com");

  // Gets a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance1->set_is_for_service_worker();
  sw_site_instance1->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();

  // Getting a RenderProcessHost for a service worker with DEFAULT reuse policy
  // should not reuse the existing service worker's process. This is because
  // we use DEFAULT reuse policy for a service worker when we have failed to
  // start the service worker and want to use a new process. We create this
  // second service worker to test the "find the newest process" logic later.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance2->set_is_for_service_worker();
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_NE(sw_host1, sw_host2);

  // Getting a RenderProcessHost for a service worker of the same site with
  // REUSE_PENDING_OR_COMMITTED_SITE reuse policy should reuse the newest
  // unmatched service worker's process (i.e., sw_host2).
  scoped_refptr<SiteInstanceImpl> sw_site_instance3 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance3->set_is_for_service_worker();
  sw_site_instance3->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  RenderProcessHost* sw_host3 = sw_site_instance3->GetProcess();
  EXPECT_EQ(sw_host2, sw_host3);

  // Getting a RenderProcessHost for a service worker of the same site with
  // REUSE_PENDING_OR_COMMITTED_SITE reuse policy should reuse the newest
  // unmatched service worker's process (i.e., sw_host2). sw_host3 doesn't cause
  // sw_host2 to be considered matched, so we can keep putting more service
  // workers in that process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance4 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance4->set_is_for_service_worker();
  sw_site_instance4->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  RenderProcessHost* sw_host4 = sw_site_instance4->GetProcess();
  EXPECT_EQ(sw_host2, sw_host4);

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host2).
  scoped_refptr<SiteInstanceImpl> site_instance1 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  EXPECT_EQ(sw_host2, site_instance1->GetProcess());

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host1). sw_host2
  // is no longer unmatched, so sw_host1 is now the newest (and only) process
  // with a corresponding unmatched service worker.
  scoped_refptr<SiteInstanceImpl> site_instance2 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  EXPECT_EQ(sw_host1, site_instance2->GetProcess());
}

TEST_F(RenderProcessHostUnitTest,
       ReuseServiceWorkerProcessInProcessPerSitePolicy) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  const GURL kUrl("http://foo.com");

  // Gets a RenderProcessHost for a service worker with process-per-site flag.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance1->set_is_for_service_worker();
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();

  // Getting a RenderProcessHost for a service worker of the same site with
  // process-per-site flag should reuse the unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance2->set_is_for_service_worker();
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_EQ(sw_host1, sw_host2);

  // Getting a RenderProcessHost for a navigation to the same site with
  // process-per-site flag should reuse the unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance3 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  RenderProcessHost* sw_host3 = sw_site_instance3->GetProcess();
  EXPECT_EQ(sw_host1, sw_host3);

  // Getting a RenderProcessHost for a navigation to the same site again with
  // process-per-site flag should reuse the unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance4 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  RenderProcessHost* sw_host4 = sw_site_instance4->GetProcess();
  EXPECT_EQ(sw_host1, sw_host4);
}

TEST_F(RenderProcessHostUnitTest, DoNotReuseOtherSiteServiceWorkerProcess) {
  const GURL kUrl1("https://foo.com");
  const GURL kUrl2("https://bar.com");

  // Gets a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  sw_site_instance1->set_is_for_service_worker();
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();

  // Getting a RenderProcessHost for a service worker of a different site should
  // return a new process because there is no reusable process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl2);
  EXPECT_NE(sw_host1, sw_site_instance2->GetProcess());
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
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl1, main_test_rfh());
  navigation->Start();
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl1);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Finish the navigation and start a new cross-site one. Getting
  // RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy should
  // return the process of the speculative RenderFrameHost.
  navigation->Commit();
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
// redirects in a renderer-initiated navigation.
TEST_F(RenderProcessHostUnitTest,
       ReuseNavigationProcessRedirectsRendererInitiated) {
  // This is only applicable to PlzNavigate.
  // TODO(clamy): This test should work with --site-per-process.
  if (!IsBrowserSideNavigationEnabled() || AreAllSitesIsolatedForTesting())
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

  // Simulate a cross-site redirect. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current
  // process for the new site since this is a renderer-intiated navigation which
  // does not swap processes on cross-site redirects. However, getting a
  // RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy should no
  // longer return the current process for the initial site we were trying to
  // navigate to.
  main_test_rfh()->SimulateRedirect(kRedirectUrl2);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kRedirectUrl2);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Once the navigation is ready to commit, Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current
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

// Tests that RenderProcessHost reuse considers navigations correctly during
// redirects in a browser-initiated navigation.
TEST_F(RenderProcessHostUnitTest,
       ReuseNavigationProcessRedirectsBrowserInitiated) {
  // This is only applicable to PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  const GURL kInitialUrl("http://google.com");
  const GURL kUrl("http://foo.com");
  const GURL kRedirectUrl1("http://foo.com/redirect");
  const GURL kRedirectUrl2("http://bar.com");

  NavigateAndCommit(kInitialUrl);

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Now getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the speculative
  // process.
  contents()->GetController().LoadURL(kUrl, Referrer(),
                                      ui::PAGE_TRANSITION_TYPED, std::string());
  main_test_rfh()->SendBeforeUnloadACK(true);
  int speculative_process_host_id =
      contents()->GetPendingMainFrame()->GetProcess()->GetID();
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(speculative_process_host_id, site_instance->GetProcess()->GetID());

  // Simulate a same-site redirect. Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the speculative
  // process.
  main_test_rfh()->SimulateRedirect(kRedirectUrl1);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(speculative_process_host_id, site_instance->GetProcess()->GetID());

  // Simulate a cross-site redirect. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should no longer return the
  // speculative process: neither for the new site nor for the initial site we
  // were trying to navigate to. It shouldn't return the current process either.
  main_test_rfh()->SimulateRedirect(kRedirectUrl2);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_NE(speculative_process_host_id, site_instance->GetProcess()->GetID());
  site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kRedirectUrl2);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_NE(speculative_process_host_id, site_instance->GetProcess()->GetID());

  // Once the navigation is ready to commit, Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the new speculative
  // process for the final site, but not the initial one. The current process
  // shouldn't be returned either.
  main_test_rfh()->PrepareForCommit();
  speculative_process_host_id =
      contents()->GetPendingMainFrame()->GetProcess()->GetID();
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_NE(speculative_process_host_id, site_instance->GetProcess()->GetID());
  site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kRedirectUrl2);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(speculative_process_host_id, site_instance->GetProcess()->GetID());
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
  TestRenderFrameHost* rfh = main_test_rfh();
  // In --site-per-process, the reload will use the pending/speculative RFH
  // instead of the current one.
  if (contents()->GetPendingMainFrame())
    rfh = contents()->GetPendingMainFrame();
  rfh->PrepareForCommit();
  rfh->SendNavigate(0, true, kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(rfh->GetProcess(), site_instance->GetProcess());

  // Remove the custom ContentBrowserClient. Site URLs are back to normal.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as it is registered
  // with the modified site URL.
  SetBrowserClientForTesting(regular_client);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_NE(rfh->GetProcess(), site_instance->GetProcess());

  // Reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it is now registered with the regular site URL.
  contents()->GetController().Reload(ReloadType::NORMAL, false);
  rfh = contents()->GetPendingMainFrame() ? contents()->GetPendingMainFrame()
                                          : main_test_rfh();
  rfh->PrepareForCommit();
  rfh->SendNavigate(0, true, kUrl);
  site_instance = SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  EXPECT_EQ(rfh->GetProcess(), site_instance->GetProcess());
}

// Tests that RenderProcessHost reuse works correctly even if the site URL of a
// URL we're navigating to changes.
TEST_F(RenderProcessHostUnitTest, ReuseExpectedSiteURLChanges) {
  // This is only applicable to PlzNavigate.
  // TODO(clamy): This test should work with --site-per-process.
  if (!IsBrowserSideNavigationEnabled() || AreAllSitesIsolatedForTesting())
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
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl, main_test_rfh());
  navigation->Start();
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
  navigation->Commit();
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

// Helper test class to modify the StoragePartition returned for a particular
// site URL.
class StoragePartitionContentBrowserClient : public ContentBrowserClient {
 public:
  StoragePartitionContentBrowserClient(const GURL& site,
                                       const std::string& partition_domain,
                                       const std::string& partition_name)
      : site_(site),
        partition_domain_(partition_domain),
        partition_name_(partition_name) {}
  ~StoragePartitionContentBrowserClient() override {}

 private:
  void GetStoragePartitionConfigForSite(BrowserContext* browser_context,
                                        const GURL& site,
                                        bool can_be_default,
                                        std::string* partition_domain,
                                        std::string* partition_name,
                                        bool* in_memory) override {
    partition_domain->clear();
    partition_name->clear();
    *in_memory = false;

    if (site == site_) {
      *partition_domain = partition_domain_;
      *partition_name = partition_name_;
    }
  }

  GURL site_;
  std::string partition_domain_;
  std::string partition_name_;
};

// Check that a SiteInstance cannot reuse a RenderProcessHost in a different
// StoragePartition.
TEST_F(RenderProcessHostUnitTest,
       DoNotReuseProcessInDifferentStoragePartition) {
  const GURL kUrl("https://foo.com");
  NavigateAndCommit(kUrl);

  // Change foo.com SiteInstances to use a different StoragePartition.
  StoragePartitionContentBrowserClient modified_client(kUrl, "foo_domain",
                                                       "foo_name");
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  // Create a foo.com SiteInstance and check that its process does not
  // reuse the foo process from the first navigation, since it's now in a
  // different StoragePartiiton.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  RenderProcessHost* process = site_instance->GetProcess();
  EXPECT_NE(main_test_rfh()->GetProcess(), process);

  SetBrowserClientForTesting(regular_client);
}

// Check that a SiteInstance cannot reuse a ServiceWorker process in a
// different StoragePartition.
TEST_F(RenderProcessHostUnitTest,
       DoNotReuseServiceWorkerProcessInDifferentStoragePartition) {
  const GURL kUrl("https://foo.com");

  // Create a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  sw_site_instance->set_is_for_service_worker();
  RenderProcessHost* sw_process = sw_site_instance->GetProcess();

  // Change foo.com SiteInstances to use a different StoragePartition.
  StoragePartitionContentBrowserClient modified_client(kUrl, "foo_domain",
                                                       "foo_name");
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  // Create a foo.com SiteInstance and check that its process does not reuse
  // the ServiceWorker foo.com process, since it's now in a different
  // StoragePartition.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl);
  site_instance->set_process_reuse_policy(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  RenderProcessHost* process = site_instance->GetProcess();
  EXPECT_NE(sw_process, process);

  SetBrowserClientForTesting(regular_client);
}

class SpareRenderProcessHostUnitTest : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    SetRenderProcessHostFactory(&rph_factory_);
    RenderViewHostImplTestHarness::SetUp();
    SetContents(NULL);  // Start with no renderers.
    while (!rph_factory_.GetProcesses()->empty()) {
      rph_factory_.Remove(rph_factory_.GetProcesses()->back().get());
    }
  }

  MockRenderProcessHostFactory rph_factory_;
};

TEST_F(SpareRenderProcessHostUnitTest, TestRendererTaken) {
  RenderProcessHost::WarmupSpareRenderProcessHost(browser_context());
  ASSERT_EQ(1U, rph_factory_.GetProcesses()->size());
  RenderProcessHost* spare_rph =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  EXPECT_EQ(spare_rph, rph_factory_.GetProcesses()->at(0).get());

  const GURL kUrl1("http://foo.com");
  SetContents(CreateTestWebContents());
  NavigateAndCommit(kUrl1);
  EXPECT_EQ(spare_rph, main_test_rfh()->GetProcess());
  ASSERT_EQ(1U, rph_factory_.GetProcesses()->size());
}

TEST_F(SpareRenderProcessHostUnitTest, TestRendererNotTaken) {
  std::unique_ptr<BrowserContext> alternate_context(new TestBrowserContext());
  RenderProcessHost::WarmupSpareRenderProcessHost(alternate_context.get());
  ASSERT_EQ(1U, rph_factory_.GetProcesses()->size());
  RenderProcessHost* spare_rph =
      RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  EXPECT_EQ(spare_rph, rph_factory_.GetProcesses()->at(0).get());

  const GURL kUrl1("http://foo.com");
  SetContents(CreateTestWebContents());
  NavigateAndCommit(kUrl1);
  EXPECT_NE(spare_rph, main_test_rfh()->GetProcess());
}

}  // namespace content

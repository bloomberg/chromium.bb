// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/streams/stream.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/browser_side_navigation_test_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

class NavigatorTestWithBrowserSideNavigation
    : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    EnableBrowserSideNavigation();
    RenderViewHostImplTestHarness::SetUp();
  }

  TestNavigationURLLoader* GetLoaderForNavigationRequest(
      NavigationRequest* request) const {
    return static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  }

  void RequestNavigation(FrameTreeNode* node, const GURL& url) {
    RequestNavigationWithParameters(node, url, Referrer(),
                                    ui::PAGE_TRANSITION_LINK,
                                    NavigationController::NO_RELOAD);
  }

  void RequestNavigationWithParameters(
      FrameTreeNode* node,
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition_type,
      NavigationController::ReloadType reload_type) {
    scoped_ptr<NavigationEntryImpl> entry(
        NavigationEntryImpl::FromNavigationEntry(
            NavigationController::CreateNavigationEntry(
                url,
                referrer,
                transition_type,
                false,
                std::string(),
                controller().GetBrowserContext())));
    static_cast<NavigatorImpl*>(node->navigator())->RequestNavigation(
        node, *entry, reload_type, base::TimeTicks::Now());
  }

  NavigationRequest* GetNavigationRequestForFrameTreeNode(
      FrameTreeNode* frame_tree_node) {
    return static_cast<NavigatorImpl*>(frame_tree_node->navigator())
        ->GetNavigationRequestForNodeForTesting(frame_tree_node);
  }

  TestRenderFrameHost* GetSpeculativeRenderFrameHost(FrameTreeNode* node) {
    return static_cast<TestRenderFrameHost*>(
        node->render_manager()->speculative_render_frame_host_.get());
  }

  // Checks if this RenderFrameHost sent a single FrameMsg_CommitNavigation
  // since the last clearing of the sink.
  // Note: caller must invoke ClearMessages on the sink at some point before
  // the tracked commit happens to clear up commit messages from previous
  // navigations.
  bool DidRenderFrameHostRequestCommit(RenderFrameHostImpl* rfh) {
    MockRenderProcessHost* rph =
        static_cast<MockRenderProcessHost*>(rfh->GetProcess());
    const FrameMsg_CommitNavigation* commit_message =
        static_cast<const FrameMsg_CommitNavigation*>(
            rph->sink().GetUniqueMessageMatching(
                FrameMsg_CommitNavigation::ID));
    return commit_message &&
           rfh->GetRoutingID() == commit_message->routing_id();
  }
};

// PlzNavigate: Test a complete browser-initiated navigation starting with a
// non-live renderer.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SimpleBrowserInitiatedNavigationFromNonLiveRenderer) {
  const GURL kUrl("http://chromium.org/");

  EXPECT_FALSE(main_test_rfh()->IsRenderFrameLive());

  // Start a browser-initiated navigation.
  int32 site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  RequestNavigation(node, kUrl);
  NavigationRequest* request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request);
  EXPECT_EQ(kUrl, request->common_params().url);
  EXPECT_TRUE(request->browser_initiated());

  // As there's no live renderer the navigation should not wait for a
  // beforeUnload ACK from the renderer and start right away.
  EXPECT_EQ(NavigationRequest::STARTED, request->state());
  ASSERT_TRUE(GetLoaderForNavigationRequest(request));
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());

  // Have the current RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request)
      ->CallOnResponseStarted(response, MakeEmptyStream());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(main_test_rfh()));
  EXPECT_EQ(NavigationRequest::RESPONSE_STARTED, request->state());

  // Commit the navigation.
  main_test_rfh()->SendNavigate(0, kUrl);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, main_test_rfh()->rfh_state());
  EXPECT_EQ(SiteInstanceImpl::GetSiteForURL(browser_context(), kUrl),
            main_test_rfh()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(kUrl, contents()->GetLastCommittedURL());
  EXPECT_FALSE(GetNavigationRequestForFrameTreeNode(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());

  // The main RenderFrameHost should not have been changed, and the renderer
  // should have been initialized.
  EXPECT_EQ(site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());

  // After a navigation is finished no speculative RenderFrameHost should
  // exist.
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // With PlzNavigate enabled a pending RenderFrameHost should never exist.
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
}

// PlzNavigate: Test a complete renderer-initiated same-site navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SimpleRendererInitiatedNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());

  // Start a renderer-initiated non-user-initiated navigation.
  process()->sink().ClearMessages();
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl2, false);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  NavigationRequest* request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request);

  // The navigation is immediately started as there's no need to wait for
  // beforeUnload to be executed.
  EXPECT_EQ(NavigationRequest::STARTED, request->state());
  EXPECT_FALSE(request->begin_params().has_user_gesture);
  EXPECT_EQ(kUrl2, request->common_params().url);
  EXPECT_FALSE(request->browser_initiated());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Have the current RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request)
      ->CallOnResponseStarted(response, MakeEmptyStream());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(main_test_rfh()));
  EXPECT_EQ(NavigationRequest::RESPONSE_STARTED, request->state());

  // Commit the navigation.
  main_test_rfh()->SendNavigate(0, kUrl2);
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, main_test_rfh()->rfh_state());
  EXPECT_EQ(SiteInstanceImpl::GetSiteForURL(browser_context(), kUrl2),
            main_test_rfh()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
  EXPECT_FALSE(GetNavigationRequestForFrameTreeNode(node));
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
}

// PlzNavigate: Test that a beforeUnload denial cancels the navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       BeforeUnloadDenialCancelNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  contents()->NavigateAndCommit(kUrl1);

  // Start a new navigation.
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  RequestNavigation(node, kUrl2);
  NavigationRequest* request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->browser_initiated());
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE, request->state());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Simulate a beforeUnload denial.
  main_test_rfh()->SendBeforeUnloadACK(false);
  EXPECT_FALSE(GetNavigationRequestForFrameTreeNode(node));
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Test that a proper NavigationRequest is created by
// RequestNavigation.
TEST_F(NavigatorTestWithBrowserSideNavigation, BeginNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  const GURL kUrl3("http://www.gmail.com/");

  contents()->NavigateAndCommit(kUrl1);

  // Add a subframe.
  FrameTreeNode* root_node = contents()->GetFrameTree()->root();
  TestRenderFrameHost* subframe_rfh = main_test_rfh()->AppendChild("Child");
  ASSERT_TRUE(subframe_rfh);

  // Start a navigation at the subframe.
  FrameTreeNode* subframe_node = subframe_rfh->frame_tree_node();
  RequestNavigation(subframe_node, kUrl2);
  NavigationRequest* subframe_request =
      GetNavigationRequestForFrameTreeNode(subframe_node);
  TestNavigationURLLoader* subframe_loader =
      GetLoaderForNavigationRequest(subframe_request);

  // Subframe navigations should start right away as they don't have to request
  // beforeUnload to run at the renderer.
  ASSERT_TRUE(subframe_request);
  ASSERT_TRUE(subframe_loader);
  EXPECT_EQ(NavigationRequest::STARTED, subframe_request->state());
  EXPECT_EQ(kUrl2, subframe_request->common_params().url);
  EXPECT_EQ(kUrl2, subframe_loader->request_info()->common_params.url);
  // First party for cookies url should be that of the main frame.
  EXPECT_EQ(kUrl1, subframe_loader->request_info()->first_party_for_cookies);
  EXPECT_FALSE(subframe_loader->request_info()->is_main_frame);
  EXPECT_TRUE(subframe_loader->request_info()->parent_is_main_frame);
  EXPECT_TRUE(subframe_request->browser_initiated());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(root_node));

  // Subframe navigations should never create a speculative RenderFrameHost,
  // unless site-per-process is enabled. In that case, as the subframe
  // navigation is to a different site and is still ongoing, it should have one.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(subframe_node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(subframe_node));
  }

  // Now start a navigation at the root node.
  RequestNavigation(root_node, kUrl3);
  NavigationRequest* main_request =
      GetNavigationRequestForFrameTreeNode(root_node);
  ASSERT_TRUE(main_request);
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            main_request->state());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(root_node));

  // Simulate a BeforeUnloadACK IPC on the main frame.
  main_test_rfh()->SendBeforeUnloadACK(true);
  TestNavigationURLLoader* main_loader =
      GetLoaderForNavigationRequest(main_request);
  EXPECT_EQ(kUrl3, main_request->common_params().url);
  EXPECT_EQ(kUrl3, main_loader->request_info()->common_params.url);
  EXPECT_EQ(kUrl3, main_loader->request_info()->first_party_for_cookies);
  EXPECT_TRUE(main_loader->request_info()->is_main_frame);
  EXPECT_FALSE(main_loader->request_info()->parent_is_main_frame);
  EXPECT_TRUE(main_request->browser_initiated());
  // BeforeUnloadACK was received from the renderer so the navigation should
  // have started.
  EXPECT_EQ(NavigationRequest::STARTED, main_request->state());

  // Main frame navigation to a different site should use a speculative
  // RenderFrameHost.
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(root_node));

  // As the main frame hasn't yet committed the subframe still exists. Thus, the
  // above situation regarding subframe navigations is valid here.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(subframe_node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(subframe_node));
  }
}

// PlzNavigate: Test that committing an HTTP 204 or HTTP 205 response cancels
// the navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation, NoContent) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Load a URL.
  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a different site.
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeforeUnloadACK(true);

  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);

  // Navigations to a different site do create a speculative RenderFrameHost.
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Commit an HTTP 204 response.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  const char kNoContentHeaders[] = "HTTP/1.1 204 No Content\0\0";
  response->head.headers = new net::HttpResponseHeaders(
      std::string(kNoContentHeaders, arraysize(kNoContentHeaders)));
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));
  EXPECT_FALSE(GetNavigationRequestForFrameTreeNode(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now, repeat the test with 205 Reset Content.

  // Navigate to a different site again.
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeforeUnloadACK(true);

  main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Commit an HTTP 205 response.
  response = new ResourceResponse;
  const char kResetContentHeaders[] = "HTTP/1.1 205 Reset Content\0\0";
  response->head.headers = new net::HttpResponseHeaders(
      std::string(kResetContentHeaders, arraysize(kResetContentHeaders)));
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));
  EXPECT_FALSE(GetNavigationRequestForFrameTreeNode(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Test that a new RenderFrameHost is created when doing a cross
// site navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation, CrossSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* initial_rfh = main_test_rfh();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a different site.
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl2);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Receive the beforeUnload ACK.
  main_test_rfh()->SendBeforeUnloadACK(true);
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));

  speculative_rfh->SendNavigate(0, kUrl2);

  RenderFrameHostImpl* final_rfh = main_test_rfh();
  EXPECT_EQ(speculative_rfh, final_rfh);
  EXPECT_NE(initial_rfh, final_rfh);
  EXPECT_TRUE(final_rfh->IsRenderFrameLive());
  EXPECT_TRUE(final_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Test that redirects are followed and the speculative
// RenderFrameHost logic behaves as expected.
TEST_F(NavigatorTestWithBrowserSideNavigation, RedirectCrossSite) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* rfh = main_test_rfh();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a URL on the same site.
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // It then redirects to another site.
  GetLoaderForNavigationRequest(main_request)->SimulateServerRedirect(kUrl2);

  // The redirect should have been followed.
  EXPECT_EQ(1, GetLoaderForNavigationRequest(main_request)->redirect_count());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());
  TestRenderFrameHost* final_speculative_rfh =
      GetSpeculativeRenderFrameHost(node);
  EXPECT_TRUE(final_speculative_rfh);
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(final_speculative_rfh));

  // Commit the navigation.
  final_speculative_rfh->SendNavigate(0, kUrl2);
  RenderFrameHostImpl* final_rfh = main_test_rfh();
  ASSERT_TRUE(final_rfh);
  EXPECT_NE(rfh, final_rfh);
  EXPECT_EQ(final_speculative_rfh, final_rfh);
  EXPECT_TRUE(final_rfh->IsRenderFrameLive());
  EXPECT_TRUE(final_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Test that a navigation is canceled if another browser-initiated
// request has been issued in the meantime. Also confirms that the speculative
// RenderFrameHost is correctly updated in the process.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       BrowserInitiatedNavigationCancel) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl1_site = SiteInstance::GetSiteForURL(browser_context(), kUrl1);
  const GURL kUrl2("http://www.google.com/");
  const GURL kUrl2_site = SiteInstance::GetSiteForURL(browser_context(), kUrl2);

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Request navigation to the 1st URL.
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* request1 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_TRUE(request1->browser_initiated());
  base::WeakPtr<TestNavigationURLLoader> loader1 =
      GetLoaderForNavigationRequest(request1)->AsWeakPtr();
  EXPECT_TRUE(loader1);

  // Confirm a speculative RenderFrameHost was created.
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32 site_instance_id_1 = speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_EQ(kUrl1_site, speculative_rfh->GetSiteInstance()->GetSiteURL());

  // Request navigation to the 2nd URL; the NavigationRequest must have been
  // replaced by a new one with a different URL.
  RequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* request2 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request2);
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_TRUE(request2->browser_initiated());

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Confirm that a new speculative RenderFrameHost was created.
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32 site_instance_id_2 = speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_NE(site_instance_id_1, site_instance_id_2);

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request2)->CallOnResponseStarted(
      response, MakeEmptyStream());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));

  // Commit the navigation.
  speculative_rfh->SendNavigate(0, kUrl2);

  // Confirm that the commit corresponds to the new request.
  ASSERT_TRUE(main_test_rfh());
  EXPECT_EQ(kUrl2_site, main_test_rfh()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());

  // Confirm that the committed RenderFrameHost is the latest speculative one.
  EXPECT_EQ(site_instance_id_2, main_test_rfh()->GetSiteInstance()->GetId());
}

// PlzNavigate: Test that a browser-initiated navigation is canceled if a
// renderer-initiated user-initiated request has been issued in the meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererUserInitiatedNavigationCancel) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a browser-initiated navigation to the 1st URL and receive its
  // beforeUnload ACK.
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* request1 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_TRUE(request1->browser_initiated());
  base::WeakPtr<TestNavigationURLLoader> loader1 =
      GetLoaderForNavigationRequest(request1)->AsWeakPtr();
  EXPECT_TRUE(loader1);

  // Confirm a speculative RenderFrameHost was created.
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32 site_instance_id_1 = speculative_rfh->GetSiteInstance()->GetId();

  // Now receive a renderer-initiated user-initiated request. It should replace
  // the current NavigationRequest.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl2, true);
  NavigationRequest* request2 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request2);
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_FALSE(request2->browser_initiated());
  EXPECT_TRUE(request2->begin_params().has_user_gesture);

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Confirm that a new speculative RenderFrameHost was created.
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32 site_instance_id_2 = speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_NE(site_instance_id_1, site_instance_id_2);

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request2)
      ->CallOnResponseStarted(response, MakeEmptyStream());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));

  // Commit the navigation.
  speculative_rfh->SendNavigate(0, kUrl2);

  // Confirm that the commit corresponds to the new request.
  ASSERT_TRUE(main_test_rfh());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());

  // Confirm that the committed RenderFrameHost is the latest speculative one.
  EXPECT_EQ(site_instance_id_2, main_test_rfh()->GetSiteInstance()->GetId());
}

// PlzNavigate: Test that a renderer-initiated user-initiated navigation is NOT
// canceled if a renderer-initiated non-user-initiated request is issued in the
// meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererNonUserInitiatedNavigationDoesntCancelRendererUserInitiated) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a renderer-initiated user-initiated navigation to the 1st URL.
  process()->sink().ClearMessages();
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl1, true);
  NavigationRequest* request1 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_FALSE(request1->browser_initiated());
  EXPECT_TRUE(request1->begin_params().has_user_gesture);
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Now receive a renderer-initiated non-user-initiated request. Nothing should
  // change.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl2, false);
  NavigationRequest* request2 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request2);
  EXPECT_EQ(request1, request2);
  EXPECT_EQ(kUrl1, request2->common_params().url);
  EXPECT_FALSE(request2->browser_initiated());
  EXPECT_TRUE(request2->begin_params().has_user_gesture);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request2)
      ->CallOnResponseStarted(response, MakeEmptyStream());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));

  // Commit the navigation.
  speculative_rfh->SendNavigate(0, kUrl1);
  EXPECT_EQ(kUrl1, contents()->GetLastCommittedURL());
}

// PlzNavigate: Test that a browser-initiated navigation is NOT canceled if a
// renderer-initiated non-user-initiated request is issued in the meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererNonUserInitiatedNavigationDoesntCancelBrowserInitiated) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a browser-initiated navigation to the 1st URL.
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl1);
  NavigationRequest* request1 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_TRUE(request1->browser_initiated());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now receive a renderer-initiated non-user-initiated request. Nothing should
  // change.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl2, false);
  NavigationRequest* request2 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request2);
  EXPECT_EQ(request1, request2);
  EXPECT_EQ(kUrl1, request2->common_params().url);
  EXPECT_TRUE(request2->browser_initiated());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now receive the beforeUnload ACK from the still ongoing navigation.
  main_test_rfh()->SendBeforeUnloadACK(true);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request2)
      ->CallOnResponseStarted(response, MakeEmptyStream());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));

  // Commit the navigation.
  speculative_rfh->SendNavigate(0, kUrl1);
  EXPECT_EQ(kUrl1, contents()->GetLastCommittedURL());
}

// PlzNavigate: Test that a renderer-initiated non-user-initiated navigation is
// canceled if a another similar request is issued in the meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererNonUserInitiatedNavigationCancelSimilarNavigation) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a renderer-initiated non-user-initiated navigation to the 1st URL.
  process()->sink().ClearMessages();
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl1, false);
  NavigationRequest* request1 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_FALSE(request1->browser_initiated());
  EXPECT_FALSE(request1->begin_params().has_user_gesture);
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  base::WeakPtr<TestNavigationURLLoader> loader1 =
      GetLoaderForNavigationRequest(request1)->AsWeakPtr();
  EXPECT_TRUE(loader1);

  // Now receive a 2nd similar request that should replace the current one.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl2, false);
  NavigationRequest* request2 = GetNavigationRequestForFrameTreeNode(node);
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_FALSE(request2->browser_initiated());
  EXPECT_FALSE(request2->begin_params().has_user_gesture);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request2)
      ->CallOnResponseStarted(response, MakeEmptyStream());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));

  // Commit the navigation.
  speculative_rfh->SendNavigate(0, kUrl2);
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
}

// PlzNavigate: Test that a reload navigation is properly signaled to the
// RenderFrame when the navigation can commit. A speculative RenderFrameHost
// should not be created at any step.
TEST_F(NavigatorTestWithBrowserSideNavigation, Reload) {
  const GURL kUrl("http://www.google.com/");
  contents()->NavigateAndCommit(kUrl);

  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  RequestNavigationWithParameters(node, kUrl, Referrer(),
                                  ui::PAGE_TRANSITION_LINK,
                                  NavigationController::RELOAD);
  // A NavigationRequest should have been generated.
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request != NULL);
  EXPECT_EQ(FrameMsg_Navigate_Type::RELOAD,
            main_request->common_params().navigation_type);
  main_test_rfh()->PrepareForCommit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  main_test_rfh()->SendNavigate(0, kUrl);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now do a shift+reload.
  RequestNavigationWithParameters(node, kUrl, Referrer(),
                                  ui::PAGE_TRANSITION_LINK,
                                  NavigationController::RELOAD_IGNORING_CACHE);
  // A NavigationRequest should have been generated.
  main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request != NULL);
  EXPECT_EQ(FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE,
            main_request->common_params().navigation_type);
  main_test_rfh()->PrepareForCommit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Confirm that a speculative RenderFrameHost is used when
// navigating from one site to another.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SpeculativeRendererWorksBaseCase) {
  // Navigate to an initial site.
  const GURL kUrlInit("http://wikipedia.org/");
  contents()->NavigateAndCommit(kUrlInit);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Begin navigating to another site.
  const GURL kUrl("http://google.com/");
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Receive the beforeUnload ACK.
  main_test_rfh()->SendBeforeUnloadACK(true);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_NE(speculative_rfh, main_test_rfh());
  EXPECT_EQ(SiteInstanceImpl::GetSiteForURL(browser_context(), kUrl),
            speculative_rfh->GetSiteInstance()->GetSiteURL());
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
  int32 site_instance_id = speculative_rfh->GetSiteInstance()->GetId();

  // Ask Navigator to commit the navigation by simulating a call to
  // OnResponseStarted.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(GetNavigationRequestForFrameTreeNode(node))
      ->CallOnResponseStarted(response, MakeEmptyStream());
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_EQ(site_instance_id, speculative_rfh->GetSiteInstance()->GetId());
  EXPECT_FALSE(node->render_manager()->pending_frame_host());

  // Invoke OnDidCommitProvisionalLoad.
  speculative_rfh->SendNavigate(0, kUrl);
  EXPECT_EQ(site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
}

// PlzNavigate: Confirm that a speculative RenderFrameHost is thrown away when
// the final URL's site differs from the initial one due to redirects.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SpeculativeRendererDiscardedAfterRedirectToAnotherSite) {
  // Navigate to an initial site.
  const GURL kUrlInit("http://wikipedia.org/");
  contents()->NavigateAndCommit(kUrlInit);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  int32 init_site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();

  // Begin navigating to another site.
  const GURL kUrl("http://google.com/");
  process()->sink().ClearMessages();
  RequestNavigation(node, kUrl);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Receive the beforeUnload ACK.
  main_test_rfh()->SendBeforeUnloadACK(true);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  int32 site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_NE(init_site_instance_id, site_instance_id);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  ASSERT_TRUE(speculative_rfh);
  EXPECT_NE(speculative_rfh, main_test_rfh());
  EXPECT_EQ(SiteInstanceImpl::GetSiteForURL(browser_context(), kUrl),
            speculative_rfh->GetSiteInstance()->GetSiteURL());

  // It then redirects to yet another site.
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);
  const GURL kUrlRedirect("https://www.google.com/");
  GetLoaderForNavigationRequest(main_request)
      ->SimulateServerRedirect(kUrlRedirect);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);

  // For now, ensure that the speculative RenderFrameHost does not change after
  // the redirect.
  // TODO(carlosk): once the speculative RenderFrameHost updates with redirects
  // this next check will be changed to verify that it actually happens.
  EXPECT_EQ(site_instance_id, speculative_rfh->GetSiteInstance()->GetId());

  // Commit the navigation with Navigator by simulating the call to
  // OnResponseStarted.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(response, MakeEmptyStream());
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(speculative_rfh));
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());

  // Once commit happens the speculative RenderFrameHost is updated to match the
  // known final SiteInstance.
  ASSERT_TRUE(speculative_rfh);
  EXPECT_EQ(SiteInstanceImpl::GetSiteForURL(browser_context(), kUrlRedirect),
            speculative_rfh->GetSiteInstance()->GetSiteURL());
  int32 redirect_site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_NE(init_site_instance_id, redirect_site_instance_id);
  EXPECT_NE(site_instance_id, redirect_site_instance_id);

  // Invoke OnDidCommitProvisionalLoad.
  speculative_rfh->SendNavigate(0, kUrlRedirect);

  // Check that the speculative RenderFrameHost was swapped in.
  EXPECT_EQ(redirect_site_instance_id,
            main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Verify that a previously swapped out RenderFrameHost is
// correctly reused when spawning a speculative RenderFrameHost in a navigation
// using the same SiteInstance.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SpeculativeRendererReuseSwappedOutRFH) {
  // Navigate to an initial site.
  const GURL kUrl1("http://wikipedia.org/");
  contents()->NavigateAndCommit(kUrl1);
  TestRenderFrameHost* rfh1 = main_test_rfh();
  FrameTreeNode* node = rfh1->frame_tree_node();
  RenderFrameHostManager* rfhm = node->render_manager();

  // Increment active frame count to cause the RenderFrameHost to be swapped out
  // (instead of immediately destroyed).
  rfh1->GetSiteInstance()->increment_active_frame_count();

  // Navigate to another site to swap out the initial RenderFrameHost.
  const GURL kUrl2("http://chromium.org/");
  contents()->NavigateAndCommit(kUrl2);
  ASSERT_NE(rfh1, main_test_rfh());
  EXPECT_NE(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, main_test_rfh()->rfh_state());
  EXPECT_TRUE(rfhm->IsOnSwappedOutList(rfh1));

  // Now go back to the initial site so that the swapped out RenderFrameHost
  // should be reused.
  process()->sink().ClearMessages();
  static_cast<MockRenderProcessHost*>(rfh1->GetProcess())
      ->sink()
      .ClearMessages();
  RequestNavigation(node, kUrl1);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  main_test_rfh()->SendBeforeUnloadACK(true);
  EXPECT_EQ(rfh1, GetSpeculativeRenderFrameHost(node));
  EXPECT_NE(RenderFrameHostImpl::STATE_DEFAULT,
            GetSpeculativeRenderFrameHost(node)->rfh_state());

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(GetNavigationRequestForFrameTreeNode(node))
      ->CallOnResponseStarted(response, MakeEmptyStream());
  EXPECT_EQ(rfh1, GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT,
            GetSpeculativeRenderFrameHost(node)->rfh_state());
  EXPECT_TRUE(DidRenderFrameHostRequestCommit(rfh1));
  EXPECT_FALSE(DidRenderFrameHostRequestCommit(main_test_rfh()));

  rfh1->SendNavigate(1, kUrl1);
  EXPECT_EQ(rfh1, main_test_rfh());
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh1->rfh_state());
  EXPECT_FALSE(rfhm->IsOnSwappedOutList(rfh1));
}

// PlzNavigate: Verify that data urls are properly handled.
TEST_F(NavigatorTestWithBrowserSideNavigation, DataUrls) {
  const GURL kUrl1("http://wikipedia.org/");
  const GURL kUrl2("data:text/html,test");

  // Navigate to an initial site.
  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a data url.
  RequestNavigation(node, kUrl2);
  NavigationRequest* navigation_request =
      GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(navigation_request);
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            navigation_request->state());
  main_test_rfh()->SendBeforeUnloadACK(true);

  // The request should not have been sent to the IO thread but committed
  // immediately.
  EXPECT_EQ(NavigationRequest::RESPONSE_STARTED,
            navigation_request->state());
  EXPECT_FALSE(navigation_request->loader_for_testing());
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  speculative_rfh->SendNavigate(0, kUrl2);
  EXPECT_EQ(main_test_rfh(), speculative_rfh);

  // Go back to the initial site.
  contents()->NavigateAndCommit(kUrl1);

  // Do a renderer-initiated navigation to a data url. The request should not be
  // sent to the IO thread, nor committed.
  TestRenderFrameHost* main_rfh = main_test_rfh();
  main_rfh->SendRendererInitiatedNavigationRequest(kUrl2, true);
  navigation_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(navigation_request);
  EXPECT_EQ(NavigationRequest::RESPONSE_STARTED,
            navigation_request->state());
  EXPECT_FALSE(navigation_request->loader_for_testing());
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  speculative_rfh->SendNavigate(0, kUrl2);
  EXPECT_EQ(main_test_rfh(), speculative_rfh);
}

}  // namespace content

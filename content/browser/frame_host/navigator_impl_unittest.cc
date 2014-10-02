// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_before_commit_info.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/navigation_params.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "ui/base/page_transition_types.h"

namespace content {

class NavigatorTest
    : public RenderViewHostImplTestHarness {
 public:
  NavigationRequest* GetNavigationRequestForFrameTreeNode(
      FrameTreeNode* frame_tree_node) const {
    NavigatorImpl* navigator =
        static_cast<NavigatorImpl*>(frame_tree_node->navigator());
    return navigator->navigation_request_map_.get(
            frame_tree_node->frame_tree_node_id());
  }

  void EnableBrowserSideNavigation() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBrowserSideNavigation);
  }

  void SendRequestNavigation(FrameTreeNode* node,
                             const GURL& url) {
    SendRequestNavigationWithParameters(
        node, url, Referrer(), ui::PAGE_TRANSITION_LINK,
        NavigationController::NO_RELOAD);
  }

  void SendRequestNavigationWithParameters(
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
};

// PlzNavigate: Test that a proper NavigationRequest is created by
// BeginNavigation.
// Note that all PlzNavigate methods on the browser side require the use of the
// flag kEnableBrowserSideNavigation.
TEST_F(NavigatorTest, BrowserSideNavigationBeginNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  const GURL kUrl3("http://www.gmail.com/");
  const int64 kFirstNavRequestID = 1;

  EnableBrowserSideNavigation();
  contents()->NavigateAndCommit(kUrl1);

  // Add a subframe.
  TestRenderFrameHost* subframe_rfh = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->AddFrame(
          contents()->GetFrameTree()->root(), 14, "Child"));

  FrameTreeNode* subframe_node = subframe_rfh->frame_tree_node();
  SendRequestNavigation(subframe_rfh->frame_tree_node(), kUrl2);
  // Simulate a BeginNavigation IPC on the subframe.
  subframe_rfh->SendBeginNavigationWithURL(kUrl2);
  NavigationRequest* subframe_request =
      GetNavigationRequestForFrameTreeNode(subframe_node);
  ASSERT_TRUE(subframe_request);
  EXPECT_EQ(kUrl2, subframe_request->common_params().url);
  // First party for cookies url should be that of the main frame.
  EXPECT_EQ(kUrl1, subframe_request->info_for_test()->first_party_for_cookies);
  EXPECT_FALSE(subframe_request->info_for_test()->is_main_frame);
  EXPECT_TRUE(subframe_request->info_for_test()->parent_is_main_frame);
  EXPECT_EQ(kFirstNavRequestID + 1, subframe_request->navigation_request_id());

  FrameTreeNode* main_frame_node =
      contents()->GetMainFrame()->frame_tree_node();
  SendRequestNavigation(main_frame_node, kUrl3);
  // Simulate a BeginNavigation IPC on the main frame.
  contents()->GetMainFrame()->SendBeginNavigationWithURL(kUrl3);
  NavigationRequest* main_request =
      GetNavigationRequestForFrameTreeNode(main_frame_node);
  ASSERT_TRUE(main_request);
  EXPECT_EQ(kUrl3, main_request->common_params().url);
  EXPECT_EQ(kUrl3, main_request->info_for_test()->first_party_for_cookies);
  EXPECT_TRUE(main_request->info_for_test()->is_main_frame);
  EXPECT_FALSE(main_request->info_for_test()->parent_is_main_frame);
  EXPECT_EQ(kFirstNavRequestID + 2, main_request->navigation_request_id());
}

// PlzNavigate: Test that RequestNavigation creates a NavigationRequest and that
// RenderFrameHost is not modified when the navigation commits.
TEST_F(NavigatorTest,
       BrowserSideNavigationRequestNavigationNoLiveRenderer) {
  const GURL kUrl("http://www.google.com/");

  EnableBrowserSideNavigation();
  EXPECT_FALSE(main_test_rfh()->render_view_host()->IsRenderViewLive());
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  SendRequestNavigation(node, kUrl);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  // A NavigationRequest should have been generated.
  EXPECT_TRUE(main_request != NULL);
  RenderFrameHostImpl* rfh = main_test_rfh();

  // Now commit the same url.
  NavigationBeforeCommitInfo commit_info;
  commit_info.navigation_url = kUrl;
  commit_info.navigation_request_id = main_request->navigation_request_id();
  node->navigator()->CommitNavigation(node, commit_info);
  main_request = GetNavigationRequestForFrameTreeNode(node);

  // The main RFH should not have been changed, and the renderer should have
  // been initialized.
  EXPECT_EQ(rfh, main_test_rfh());
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());
  EXPECT_TRUE(main_test_rfh()->render_view_host()->IsRenderViewLive());
}

// PlzNavigate: Test that a new RenderFrameHost is created when doing a cross
// site navigation.
TEST_F(NavigatorTest,
       BrowserSideNavigationCrossSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  EnableBrowserSideNavigation();
  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* rfh = main_test_rfh();
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh->rfh_state());
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a different site.
  SendRequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl2);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);

  NavigationBeforeCommitInfo commit_info;
  commit_info.navigation_url = kUrl2;
  commit_info.navigation_request_id = main_request->navigation_request_id();
  node->navigator()->CommitNavigation(node, commit_info);
  EXPECT_NE(main_test_rfh(), rfh);
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());
  EXPECT_TRUE(main_test_rfh()->render_view_host()->IsRenderViewLive());
}

// PlzNavigate: Test that a navigation commit is ignored if another request has
// been issued in the meantime.
// TODO(carlosk): add checks to assert that the cancel call was sent to
// ResourceDispatcherHost in the IO thread by extending
// ResourceDispatcherHostDelegate (like in cross_site_transfer_browsertest.cc
// and plugin_browsertest.cc).
TEST_F(NavigatorTest,
       BrowserSideNavigationIgnoreStaleNavigationCommit) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl0_site = SiteInstance::GetSiteForURL(browser_context(), kUrl0);
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");
  const GURL kUrl2_site = SiteInstance::GetSiteForURL(browser_context(), kUrl2);

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  EnableBrowserSideNavigation();
  EXPECT_EQ(kUrl0_site, main_test_rfh()->GetSiteInstance()->GetSiteURL());

  // Request navigation to the 1st URL and gather data.
  SendRequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl1);
  NavigationRequest* request1 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request1);
  int64 request_id1 = request1->navigation_request_id();

  // Request navigation to the 2nd URL and gather more data.
  SendRequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl2);
  NavigationRequest* request2 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request2);
  int64 request_id2 = request2->navigation_request_id();
  EXPECT_NE(request_id1, request_id2);

  // Confirms that a stale commit is ignored by the Navigator.
  NavigationBeforeCommitInfo nbc_info;
  nbc_info.navigation_url = kUrl1;
  nbc_info.navigation_request_id = request_id1;
  node->navigator()->CommitNavigation(node, nbc_info);
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
  EXPECT_EQ(kUrl0_site, main_test_rfh()->GetSiteInstance()->GetSiteURL());

  // Confirms that a valid, request-matching commit is correctly processed.
  nbc_info.navigation_url = kUrl2;
  nbc_info.navigation_request_id = request_id2;
  node->navigator()->CommitNavigation(node, nbc_info);
  RenderFrameHostImpl* pending_rfh =
      node->render_manager()->pending_frame_host();
  ASSERT_TRUE(pending_rfh);
  EXPECT_EQ(kUrl2_site, pending_rfh->GetSiteInstance()->GetSiteURL());
}

// PlzNavigate: Tests that the navigation histograms are correctly tracked both
// when PlzNavigate is enabled and disabled, and also ignores in-tab renderer
// initiated navigation for the non-enabled case.
// Note: the related histogram, Navigation.TimeToURLJobStart, cannot be tracked
// by this test as the IO thread is not running.
TEST_F(NavigatorTest, BrowserSideNavigationHistogramTest) {
  const GURL kUrl0("http://www.google.com/");
  const GURL kUrl1("http://www.chromium.org/");
  base::HistogramTester histo_tester;

  // Performs a "normal" non-PlzNavigate navigation
  contents()->NavigateAndCommit(kUrl0);
  histo_tester.ExpectTotalCount("Navigation.TimeToCommit", 1);

  // Performs an in-tab renderer initiated navigation
  int32 new_page_id = 1 + contents()->GetMaxPageIDForSiteInstance(
      main_test_rfh()->GetSiteInstance());
  main_test_rfh()->SendNavigate(new_page_id, kUrl0);
  histo_tester.ExpectTotalCount("Navigation.TimeToCommit", 1);

  // Performs a PlzNavigate navigation
  EnableBrowserSideNavigation();
  contents()->NavigateAndCommit(kUrl1);
  histo_tester.ExpectTotalCount("Navigation.TimeToCommit", 2);
}

// PlzNavigate: Test that a reload navigation is properly signaled to the
// renderer when the navigation can commit.
TEST_F(NavigatorTest, BrowserSideNavigationReload) {
  const GURL kUrl("http://www.google.com/");
  contents()->NavigateAndCommit(kUrl);

  EnableBrowserSideNavigation();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  SendRequestNavigationWithParameters(
      node, kUrl, Referrer(), ui::PAGE_TRANSITION_LINK,
      NavigationController::RELOAD);
  contents()->GetMainFrame()->SendBeginNavigationWithURL(kUrl);
  // A NavigationRequest should have been generated.
  NavigationRequest* main_request =
      GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request != NULL);
  EXPECT_EQ(FrameMsg_Navigate_Type::RELOAD,
            main_request->common_params().navigation_type);
  int page_id = contents()->GetMaxPageIDForSiteInstance(
                    main_test_rfh()->GetSiteInstance()) + 1;
  main_test_rfh()->SendNavigate(page_id, kUrl);

  // Now do a shift+reload.
  SendRequestNavigationWithParameters(
      node, kUrl, Referrer(), ui::PAGE_TRANSITION_LINK,
      NavigationController::RELOAD_IGNORING_CACHE);
  contents()->GetMainFrame()->SendBeginNavigationWithURL(kUrl);
  // A NavigationRequest should have been generated.
  main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request != NULL);
  EXPECT_EQ(FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE,
            main_request->common_params().navigation_type);
}

}  // namespace content

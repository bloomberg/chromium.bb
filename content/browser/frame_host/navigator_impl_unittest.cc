// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/navigation_url_loader_factory.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_registry.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

namespace {

class TestNavigationURLLoader
    : public NavigationURLLoader,
      public base::SupportsWeakPtr<TestNavigationURLLoader> {
 public:
  TestNavigationURLLoader(const CommonNavigationParams& common_params,
                          scoped_ptr<NavigationRequestInfo> request_info,
                          NavigationURLLoaderDelegate* delegate)
      : common_params_(common_params),
        request_info_(request_info.Pass()),
        delegate_(delegate),
        redirect_count_(0) {
  }

  // NavigationURLLoader implementation.
  void FollowRedirect() override { redirect_count_++; }

  const CommonNavigationParams& common_params() const { return common_params_; }
  NavigationRequestInfo* request_info() const { return request_info_.get(); }

  void CallOnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<ResourceResponse>& response) {
    delegate_->OnRequestRedirected(redirect_info, response);
  }

  void CallOnResponseStarted(
      const scoped_refptr<ResourceResponse>& response,
      scoped_ptr<StreamHandle> body) {
    delegate_->OnResponseStarted(response, body.Pass());
  }

  int redirect_count() { return redirect_count_; }

 private:
  CommonNavigationParams common_params_;
  scoped_ptr<NavigationRequestInfo> request_info_;
  NavigationURLLoaderDelegate* delegate_;
  int redirect_count_;
};

class TestNavigationURLLoaderFactory : public NavigationURLLoaderFactory {
 public:
  // NavigationURLLoaderFactory implementation.
  scoped_ptr<NavigationURLLoader> CreateLoader(
      BrowserContext* browser_context,
      int64 frame_tree_node_id,
      const CommonNavigationParams& common_params,
      scoped_ptr<NavigationRequestInfo> request_info,
      ResourceRequestBody* request_body,
      NavigationURLLoaderDelegate* delegate) override {
    return scoped_ptr<NavigationURLLoader>(new TestNavigationURLLoader(
        common_params, request_info.Pass(), delegate));
  }
};

}  // namespace

class NavigatorTest : public RenderViewHostImplTestHarness {
 public:
  NavigatorTest() : stream_registry_(new StreamRegistry) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    loader_factory_.reset(new TestNavigationURLLoaderFactory);
    NavigationURLLoader::SetFactoryForTesting(loader_factory_.get());
  }

  void TearDown() override {
    NavigationURLLoader::SetFactoryForTesting(nullptr);
    loader_factory_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  NavigationRequest* GetNavigationRequestForFrameTreeNode(
      FrameTreeNode* frame_tree_node) const {
    NavigatorImpl* navigator =
        static_cast<NavigatorImpl*>(frame_tree_node->navigator());
    return navigator->navigation_request_map_.get(
            frame_tree_node->frame_tree_node_id());
  }

  TestNavigationURLLoader* GetLoaderForNavigationRequest(
      NavigationRequest* request) const {
    return static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
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

  scoped_ptr<StreamHandle> MakeEmptyStream() {
    GURL url(std::string(url::kBlobScheme) + "://" + base::GenerateGUID());
    scoped_refptr<Stream> stream(new Stream(stream_registry_.get(), NULL, url));
    stream->Finalize();
    return stream->CreateHandle();
  }

 private:
  scoped_ptr<StreamRegistry> stream_registry_;
  scoped_ptr<TestNavigationURLLoaderFactory> loader_factory_;
};

// PlzNavigate: Test that a proper NavigationRequest is created by
// BeginNavigation.
// Note that all PlzNavigate methods on the browser side require the use of the
// flag kEnableBrowserSideNavigation.
TEST_F(NavigatorTest, BrowserSideNavigationBeginNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  const GURL kUrl3("http://www.gmail.com/");

  contents()->NavigateAndCommit(kUrl1);

  EnableBrowserSideNavigation();

  // Add a subframe.
  FrameTreeNode* root = contents()->GetFrameTree()->root();
  TestRenderFrameHost* subframe_rfh = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->AddFrame(
          root, root->current_frame_host()->GetProcess()->GetID(), 14,
          "Child"));
  EXPECT_TRUE(subframe_rfh);

  FrameTreeNode* subframe_node = subframe_rfh->frame_tree_node();
  SendRequestNavigation(subframe_rfh->frame_tree_node(), kUrl2);
  // There is no previous renderer in the subframe, so BeginNavigation is
  // handled already.
  NavigationRequest* subframe_request =
      GetNavigationRequestForFrameTreeNode(subframe_node);
  TestNavigationURLLoader* subframe_loader =
      GetLoaderForNavigationRequest(subframe_request);
  ASSERT_TRUE(subframe_request);
  EXPECT_EQ(kUrl2, subframe_request->common_params().url);
  EXPECT_EQ(kUrl2, subframe_loader->common_params().url);
  // First party for cookies url should be that of the main frame.
  EXPECT_EQ(kUrl1, subframe_loader->request_info()->first_party_for_cookies);
  EXPECT_FALSE(subframe_loader->request_info()->is_main_frame);
  EXPECT_TRUE(subframe_loader->request_info()->parent_is_main_frame);

  SendRequestNavigation(root, kUrl3);
  // Simulate a BeginNavigation IPC on the main frame.
  contents()->GetMainFrame()->SendBeginNavigationWithURL(kUrl3);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(root);
  TestNavigationURLLoader* main_loader =
      GetLoaderForNavigationRequest(main_request);
  ASSERT_TRUE(main_request);
  EXPECT_EQ(kUrl3, main_request->common_params().url);
  EXPECT_EQ(kUrl3, main_loader->common_params().url);
  EXPECT_EQ(kUrl3, main_loader->request_info()->first_party_for_cookies);
  EXPECT_TRUE(main_loader->request_info()->is_main_frame);
  EXPECT_FALSE(main_loader->request_info()->parent_is_main_frame);
}

// PlzNavigate: Test that RequestNavigation creates a NavigationRequest and that
// RenderFrameHost is not modified when the navigation commits.
TEST_F(NavigatorTest, BrowserSideNavigationRequestNavigationNoLiveRenderer) {
  const GURL kUrl("http://www.google.com/");

  EnableBrowserSideNavigation();
  EXPECT_FALSE(main_test_rfh()->render_view_host()->IsRenderViewLive());
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  SendRequestNavigation(node, kUrl);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  // A NavigationRequest should have been generated.
  EXPECT_TRUE(main_request != NULL);
  RenderFrameHostImpl* rfh = main_test_rfh();

  // Now return the response without any redirects. This will cause the
  // navigation to commit at the same URL.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());
  main_request = GetNavigationRequestForFrameTreeNode(node);

  // The main RFH should not have been changed, and the renderer should have
  // been initialized.
  EXPECT_EQ(rfh, main_test_rfh());
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());
  EXPECT_TRUE(main_test_rfh()->render_view_host()->IsRenderViewLive());
}

// PlzNavigate: Test that commiting an HTTP 204 or HTTP 205 response cancels the
// navigation.
TEST_F(NavigatorTest, BrowserSideNavigationNoContent) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Load a URL.
  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* rfh = main_test_rfh();
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh->rfh_state());
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  EnableBrowserSideNavigation();

  // Navigate to a different site.
  SendRequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl2);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);

  // Commit an HTTP 204 response.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  const char kNoContentHeaders[] = "HTTP/1.1 204 No Content\0\0";
  response->head.headers = new net::HttpResponseHeaders(
      std::string(kNoContentHeaders, arraysize(kNoContentHeaders)));
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());

  // There should be no pending RenderFrameHost; the navigation was aborted.
  EXPECT_FALSE(GetNavigationRequestForFrameTreeNode(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());

  // Now, repeat the test with 205 Reset Content.

  // Navigate to a different site again.
  SendRequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl2);
  main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);

  // Commit an HTTP 205 response.
  response = new ResourceResponse;
  const char kResetContentHeaders[] = "HTTP/1.1 205 Reset Content\0\0";
  response->head.headers = new net::HttpResponseHeaders(
      std::string(kResetContentHeaders, arraysize(kResetContentHeaders)));
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());

  // There should be no pending RenderFrameHost; the navigation was aborted.
  EXPECT_FALSE(GetNavigationRequestForFrameTreeNode(node));
  EXPECT_FALSE(node->render_manager()->pending_frame_host());
}

// PlzNavigate: Test that a new RenderFrameHost is created when doing a cross
// site navigation.
TEST_F(NavigatorTest, BrowserSideNavigationCrossSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* rfh = main_test_rfh();
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh->rfh_state());
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  EnableBrowserSideNavigation();

  // Navigate to a different site.
  SendRequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl2);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());
  RenderFrameHostImpl* pending_rfh =
      node->render_manager()->pending_frame_host();
  ASSERT_TRUE(pending_rfh);
  EXPECT_NE(pending_rfh, rfh);
  EXPECT_TRUE(pending_rfh->IsRenderFrameLive());
  EXPECT_TRUE(pending_rfh->render_view_host()->IsRenderViewLive());
}

// PlzNavigate: Test that redirects are followed.
TEST_F(NavigatorTest, BrowserSideNavigationRedirectCrossSite) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* rfh = main_test_rfh();
  EXPECT_EQ(RenderFrameHostImpl::STATE_DEFAULT, rfh->rfh_state());
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  EnableBrowserSideNavigation();

  // Navigate to a URL on the same site.
  SendRequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl1);
  NavigationRequest* main_request = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(main_request);

  // It then redirects to another site.
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = kUrl2;
  redirect_info.new_first_party_for_cookies = kUrl2;
 scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(main_request)->CallOnRequestRedirected(
      redirect_info, response);

  // The redirect should have been followed.
  EXPECT_EQ(1, GetLoaderForNavigationRequest(main_request)->redirect_count());

  // Then it commits.
  response = new ResourceResponse;
  GetLoaderForNavigationRequest(main_request)->CallOnResponseStarted(
      response, MakeEmptyStream());
  RenderFrameHostImpl* pending_rfh =
      node->render_manager()->pending_frame_host();
  ASSERT_TRUE(pending_rfh);
  EXPECT_NE(pending_rfh, rfh);
  EXPECT_TRUE(pending_rfh->IsRenderFrameLive());
  EXPECT_TRUE(pending_rfh->render_view_host()->IsRenderViewLive());
}

// PlzNavigate: Test that a navigation is cancelled if another request has been
// issued in the meantime.
TEST_F(NavigatorTest, BrowserSideNavigationReplacePendingNavigation) {
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

  // Request navigation to the 1st URL.
  SendRequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl1);
  NavigationRequest* request1 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  base::WeakPtr<TestNavigationURLLoader> loader1 =
      GetLoaderForNavigationRequest(request1)->AsWeakPtr();

  // Request navigation to the 2nd URL; the NavigationRequest must have been
  // replaced by a new one with a different URL.
  SendRequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeginNavigationWithURL(kUrl2);
  NavigationRequest* request2 = GetNavigationRequestForFrameTreeNode(node);
  ASSERT_TRUE(request2);
  EXPECT_EQ(kUrl2, request2->common_params().url);

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Confirm that the commit corresponds to the new request.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  GetLoaderForNavigationRequest(request2)->CallOnResponseStarted(
      response, MakeEmptyStream());
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

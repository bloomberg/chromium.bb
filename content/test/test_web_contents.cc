// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_web_contents.h"

#include <utility>

#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/page_state.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "ui/base/page_transition_types.h"

namespace content {

TestWebContents::TestWebContents(BrowserContext* browser_context)
    : WebContentsImpl(browser_context),
      delegate_view_override_(NULL),
      expect_set_history_offset_and_length_(false),
      expect_set_history_offset_and_length_history_length_(0) {
}

TestWebContents* TestWebContents::Create(BrowserContext* browser_context,
                                         scoped_refptr<SiteInstance> instance) {
  TestWebContents* test_web_contents = new TestWebContents(browser_context);
  test_web_contents->Init(
      WebContents::CreateParams(browser_context, std::move(instance)));
  return test_web_contents;
}

TestWebContents::~TestWebContents() {
  EXPECT_FALSE(expect_set_history_offset_and_length_);
}

TestRenderFrameHost* TestWebContents::GetMainFrame() {
  return static_cast<TestRenderFrameHost*>(WebContentsImpl::GetMainFrame());
}

TestRenderViewHost* TestWebContents::GetRenderViewHost() const {
    return static_cast<TestRenderViewHost*>(
        WebContentsImpl::GetRenderViewHost());
}

TestRenderFrameHost* TestWebContents::GetPendingMainFrame() const {
  if (IsBrowserSideNavigationEnabled()) {
    return static_cast<TestRenderFrameHost*>(
        GetRenderManager()->speculative_render_frame_host_.get());
  }
  return static_cast<TestRenderFrameHost*>(
      GetRenderManager()->pending_frame_host());
}

int TestWebContents::DownloadImage(const GURL& url,
                                   bool is_favicon,
                                   uint32_t max_bitmap_size,
                                   bool bypass_cache,
                                   const ImageDownloadCallback& callback) {
  static int g_next_image_download_id = 0;
  ++g_next_image_download_id;
  pending_image_downloads_[url].emplace_back(g_next_image_download_id,
                                             callback);
  return g_next_image_download_id;
}

const GURL& TestWebContents::GetLastCommittedURL() const {
  if (last_committed_url_.is_valid()) {
    return last_committed_url_;
  }
  return WebContentsImpl::GetLastCommittedURL();
}

void TestWebContents::TestDidNavigate(RenderFrameHost* render_frame_host,
                                      int nav_entry_id,
                                      bool did_create_new_entry,
                                      const GURL& url,
                                      ui::PageTransition transition) {
  TestDidNavigateWithReferrer(render_frame_host,
                              nav_entry_id,
                              did_create_new_entry,
                              url,
                              Referrer(),
                              transition);
}

void TestWebContents::TestDidNavigateWithReferrer(
    RenderFrameHost* render_frame_host,
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition) {
  TestDidNavigateWithSequenceNumber(render_frame_host, nav_entry_id,
                                    did_create_new_entry, url, referrer,
                                    transition, false, -1, -1);
}

void TestWebContents::TestDidNavigateWithSequenceNumber(
    RenderFrameHost* render_frame_host,
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    bool was_within_same_document,
    int item_sequence_number,
    int document_sequence_number) {
  TestRenderFrameHost* rfh =
      static_cast<TestRenderFrameHost*>(render_frame_host);
  rfh->InitializeRenderFrameIfNeeded();

  if (!rfh->is_loading())
    rfh->SimulateNavigationStart(url);

  FrameHostMsg_DidCommitProvisionalLoad_Params params;

  params.nav_entry_id = nav_entry_id;
  params.frame_unique_name = std::string();
  params.item_sequence_number = item_sequence_number;
  params.document_sequence_number = document_sequence_number;
  params.url = url;
  params.base_url = GURL();
  params.referrer = referrer;
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = true;
  params.contents_mime_type = std::string("text/html");
  params.socket_address = net::HostPortPair();
  params.intended_as_new_entry = did_create_new_entry;
  params.did_create_new_entry = did_create_new_entry;
  params.should_replace_current_entry = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.post_id = 0;
  params.was_within_same_document = was_within_same_document;
  params.http_status_code = 200;
  params.url_is_unreachable = false;
  if (item_sequence_number != -1 && document_sequence_number != -1) {
    params.page_state = PageState::CreateForTestingWithSequenceNumbers(
        url, item_sequence_number, document_sequence_number);
  } else {
    params.page_state = PageState::CreateFromURL(url);
  }
  params.original_request_url = GURL();
  params.is_overriding_user_agent = false;
  params.history_list_was_cleared = false;
  params.render_view_routing_id = 0;
  params.origin = url::Origin();
  params.report_type = FrameMsg_UILoadMetricsReportType::NO_REPORT;
  params.ui_timestamp = base::TimeTicks();
  params.insecure_request_policy = blink::kLeaveInsecureRequestsAlone;
  params.has_potentially_trustworthy_unique_origin = false;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();

  rfh->SendNavigateWithParams(&params);
}

const std::string& TestWebContents::GetSaveFrameHeaders() {
  return save_frame_headers_;
}

bool TestWebContents::HasPendingDownloadImage(const GURL& url) {
  return !pending_image_downloads_[url].empty();
}

bool TestWebContents::TestDidDownloadImage(
    const GURL& url,
    int http_status_code,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if (!HasPendingDownloadImage(url))
    return false;
  int id = pending_image_downloads_[url].front().first;
  ImageDownloadCallback callback = pending_image_downloads_[url].front().second;
  pending_image_downloads_[url].pop_front();
  callback.Run(id, http_status_code, url, bitmaps, original_bitmap_sizes);
  return true;
}

void TestWebContents::SetLastCommittedURL(const GURL& url) {
  last_committed_url_ = url;
}

bool TestWebContents::CrossProcessNavigationPending() {
  if (IsBrowserSideNavigationEnabled()) {
    return GetRenderManager()->speculative_render_frame_host_ != nullptr;
  }
  return GetRenderManager()->pending_frame_host() != nullptr;
}

bool TestWebContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host,
    int opener_frame_routing_id,
    int proxy_routing_id,
    const FrameReplicationState& replicated_frame_state) {
  // This will go to a TestRenderViewHost.
  static_cast<RenderViewHostImpl*>(render_view_host)
      ->CreateRenderView(opener_frame_routing_id, proxy_routing_id,
                         replicated_frame_state, false);
  return true;
}

WebContents* TestWebContents::Clone() {
  WebContentsImpl* contents =
      Create(GetBrowserContext(), SiteInstance::Create(GetBrowserContext()));
  contents->GetController().CopyStateFrom(controller_, true);
  return contents;
}

void TestWebContents::NavigateAndCommit(const GURL& url) {
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(url, this);
  // TODO(clamy): Browser-initiated navigations should not have a transition of
  // type ui::PAGE_TRANSITION_LINK however several tests expect this. They
  // should be rewritten to simulate renderer-initiated navigations in these
  // cases. Once that's done, the transtion can be set to
  // ui::PAGE_TRANSITION_TYPED which makes more sense in this context.
  navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation->Commit();
}

void TestWebContents::TestSetIsLoading(bool value) {
  if (value) {
    DidStartLoading(GetMainFrame()->frame_tree_node(), true);
  } else {
    for (FrameTreeNode* node : frame_tree_.Nodes()) {
      RenderFrameHostImpl* current_frame_host =
          node->render_manager()->current_frame_host();
      DCHECK(current_frame_host);
      current_frame_host->ResetLoadingState();

      if (IsBrowserSideNavigationEnabled()) {
        RenderFrameHostImpl* speculative_frame_host =
            node->render_manager()->speculative_frame_host();
        if (speculative_frame_host)
          speculative_frame_host->ResetLoadingState();
        node->ResetNavigationRequest(false, true);
      } else {
        RenderFrameHostImpl* pending_frame_host =
            node->render_manager()->pending_frame_host();
        if (pending_frame_host)
          pending_frame_host->ResetLoadingState();
      }
    }
  }
}

void TestWebContents::CommitPendingNavigation() {
  const NavigationEntry* entry = GetController().GetPendingEntry();
  DCHECK(entry);

  TestRenderFrameHost* old_rfh = GetMainFrame();

  // PlzNavigate: the pending RenderFrameHost is not created before the
  // navigation commit, so it is necessary to simulate the IO thread response
  // here to commit in the proper renderer. It is necessary to call
  // PrepareForCommit before getting the main and the pending frame because when
  // we are trying to navigate to a webui from a new tab, a RenderFrameHost is
  // created to display it that is committed immediately (since it is a new
  // tab). Therefore the main frame is replaced without a pending frame being
  // created, and we don't get the right values for the RenderFrameHost to
  // navigate: we try to use the old one that has been deleted in the meantime.
  // Note that for some synchronous navigations (about:blank, javascript urls,
  // etc.), no simulation of the network stack is required.
  old_rfh->PrepareForCommitIfNecessary();

  TestRenderFrameHost* rfh = GetPendingMainFrame();
  if (!rfh)
    rfh = old_rfh;
  const bool browser_side_navigation = IsBrowserSideNavigationEnabled();
  CHECK(!browser_side_navigation || rfh->is_loading() ||
        IsRendererDebugURL(entry->GetURL()));
  CHECK(!browser_side_navigation ||
        !rfh->frame_tree_node()->navigation_request());

  rfh->SendNavigateWithTransition(entry->GetUniqueID(),
                                  GetController().GetPendingEntryIndex() == -1,
                                  entry->GetURL(), entry->GetTransitionType());
  // Simulate the SwapOut_ACK. This is needed when cross-site navigation
  // happens.
  if (old_rfh != rfh)
    old_rfh->OnSwappedOut();
}

void TestWebContents::ProceedWithCrossSiteNavigation() {
  if (!GetPendingMainFrame())
    return;
  GetMainFrame()->SendBeforeUnloadACK(true);
}

RenderViewHostDelegateView* TestWebContents::GetDelegateView() {
  if (delegate_view_override_)
    return delegate_view_override_;
  return WebContentsImpl::GetDelegateView();
}

void TestWebContents::SetOpener(TestWebContents* opener) {
  frame_tree_.root()->SetOpener(opener->GetFrameTree()->root());
}

void TestWebContents::AddPendingContents(TestWebContents* contents) {
  // This is normally only done in WebContentsImpl::CreateNewWindow.
  ProcessRoutingIdPair key(contents->GetRenderViewHost()->GetProcess()->GetID(),
                           contents->GetRenderViewHost()->GetRoutingID());
  pending_contents_[key] = contents;
  AddDestructionObserver(contents);
}

void TestWebContents::ExpectSetHistoryOffsetAndLength(int history_offset,
                                                      int history_length) {
  expect_set_history_offset_and_length_ = true;
  expect_set_history_offset_and_length_history_offset_ = history_offset;
  expect_set_history_offset_and_length_history_length_ = history_length;
}

void TestWebContents::SetHistoryOffsetAndLength(int history_offset,
                                                int history_length) {
  EXPECT_TRUE(expect_set_history_offset_and_length_);
  expect_set_history_offset_and_length_ = false;
  EXPECT_EQ(expect_set_history_offset_and_length_history_offset_,
            history_offset);
  EXPECT_EQ(expect_set_history_offset_and_length_history_length_,
            history_length);
}

void TestWebContents::TestDidFinishLoad(const GURL& url) {
  FrameHostMsg_DidFinishLoad msg(0, url);
  frame_tree_.root()->current_frame_host()->OnMessageReceived(msg);
}

void TestWebContents::TestDidFailLoadWithError(
    const GURL& url,
    int error_code,
    const base::string16& error_description) {
  FrameHostMsg_DidFailLoadWithError msg(0, url, error_code, error_description);
  frame_tree_.root()->current_frame_host()->OnMessageReceived(msg);
}

void TestWebContents::SetNavigationData(
    NavigationHandle* navigation_handle,
    std::unique_ptr<NavigationData> navigation_data) {
  static_cast<NavigationHandleImpl*>(navigation_handle)
      ->set_navigation_data(std::move(navigation_data));
}

void TestWebContents::SetHttpResponseHeaders(
    NavigationHandle* navigation_handle,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  static_cast<NavigationHandleImpl*>(navigation_handle)
      ->set_response_headers_for_testing(response_headers);
}

void TestWebContents::CreateNewWindow(
    RenderFrameHost* opener,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    const mojom::CreateNewWindowParams& params,
    SessionStorageNamespace* session_storage_namespace) {}

void TestWebContents::CreateNewWidget(int32_t render_process_id,
                                      int32_t route_id,
                                      mojom::WidgetPtr widget,
                                      blink::WebPopupType popup_type) {}

void TestWebContents::CreateNewFullscreenWidget(int32_t render_process_id,
                                                int32_t route_id,
                                                mojom::WidgetPtr widget) {}

void TestWebContents::ShowCreatedWindow(int process_id,
                                        int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_rect,
                                        bool user_gesture) {
}

void TestWebContents::ShowCreatedWidget(int process_id,
                                        int route_id,
                                        const gfx::Rect& initial_rect) {
}

void TestWebContents::ShowCreatedFullscreenWidget(int process_id,
                                                  int route_id) {
}

void TestWebContents::SaveFrameWithHeaders(const GURL& url,
                                           const Referrer& referrer,
                                           const std::string& headers) {
  save_frame_headers_ = headers;
}

void TestWebContents::SetWasRecentlyAudible(bool audible) {
  audio_stream_monitor()->set_was_recently_audible_for_testing(audible);
}

}  // namespace content

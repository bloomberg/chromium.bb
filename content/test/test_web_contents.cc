// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_web_contents.h"

#include <utility>

#include "base/command_line.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_state.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_render_view_host.h"
#include "ui/base/page_transition_types.h"

namespace content {

TestWebContents::TestWebContents(BrowserContext* browser_context)
    : WebContentsImpl(browser_context, NULL),
      delegate_view_override_(NULL),
      expect_set_history_offset_and_length_(false),
      expect_set_history_offset_and_length_history_length_(0) {
}

TestWebContents* TestWebContents::Create(BrowserContext* browser_context,
                                         SiteInstance* instance) {
  TestWebContents* test_web_contents = new TestWebContents(browser_context);
  test_web_contents->Init(WebContents::CreateParams(browser_context, instance));
  test_web_contents->RenderFrameCreated(test_web_contents->GetMainFrame());
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    return static_cast<TestRenderFrameHost*>(
        GetRenderManager()->speculative_render_frame_host_.get());
  }
  return static_cast<TestRenderFrameHost*>(
      GetRenderManager()->pending_frame_host());
}

void TestWebContents::TestDidNavigate(RenderFrameHost* render_frame_host,
                                      int page_id,
                                      const GURL& url,
                                      ui::PageTransition transition) {
  TestDidNavigateWithReferrer(render_frame_host,
                              page_id,
                              url,
                              Referrer(),
                              transition);
}

void TestWebContents::TestDidNavigateWithReferrer(
    RenderFrameHost* render_frame_host,
    int page_id,
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition) {
  FrameHostMsg_DidCommitProvisionalLoad_Params params;

  params.page_id = page_id;
  params.url = url;
  params.referrer = referrer;
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = false;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.was_within_same_page = false;
  params.is_post = false;
  params.page_state = PageState::CreateFromURL(url);

  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  rfhi->frame_tree_node()->navigator()->DidNavigate(rfhi, params);
}

bool TestWebContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host,
    int opener_route_id,
    int proxy_routing_id,
    bool for_main_frame) {
  UpdateMaxPageIDIfNecessary(render_view_host);
  // This will go to a TestRenderViewHost.
  static_cast<RenderViewHostImpl*>(
      render_view_host)->CreateRenderView(base::string16(),
                                          opener_route_id,
                                          proxy_routing_id,
                                          -1, false);
  return true;
}

WebContents* TestWebContents::Clone() {
  WebContentsImpl* contents =
      Create(GetBrowserContext(), SiteInstance::Create(GetBrowserContext()));
  contents->GetController().CopyStateFrom(controller_);
  return contents;
}

void TestWebContents::NavigateAndCommit(const GURL& url) {
  GetController().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
      &loaded_url, GetBrowserContext(), &reverse_on_redirect);

  // LoadURL created a navigation entry, now simulate the RenderView sending
  // a notification that it actually navigated.
  CommitPendingNavigation();
}

void TestWebContents::TestSetIsLoading(bool value) {
  SetIsLoading(value, true, nullptr);
}

void TestWebContents::CommitPendingNavigation() {
  const NavigationEntry* entry = GetController().GetPendingEntry();
  DCHECK(entry);

  // If we are doing a cross-site navigation, this simulates the current RFH
  // notifying that it has unloaded so the pending RFH is resumed and can
  // navigate.
  // PlzNavigate: the pending RFH is not created before the navigation commit,
  // so it is necessary to simulate the IO thread response here to commit in the
  // proper renderer. It is necessary to call PrepareForCommit before getting
  // the main and the pending frame because when we are trying to navigate to a
  // webui from a new tab, a RenderFrameHost is created to display it that is
  // committed immediately (since it is a new tab). Therefore the main frame is
  // replaced without a pending frame being created, and we don't get the right
  // values for the RFH to navigate: we try to use the old one that has been
  // deleted in the meantime.
  GetMainFrame()->PrepareForCommit();

  TestRenderFrameHost* old_rfh = GetMainFrame();
  TestRenderFrameHost* rfh = GetPendingMainFrame();
  if (!rfh)
    rfh = old_rfh;

  int page_id = entry->GetPageID();
  if (page_id == -1) {
    // It's a new navigation, assign a never-seen page id to it.
    page_id = GetMaxPageIDForSiteInstance(rfh->GetSiteInstance()) + 1;
  }

  rfh->SendNavigate(page_id, entry->GetURL());
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
  // This is normally only set in the WebContents constructor, which also
  // registers an observer for when the opener gets closed.
  opener_ = opener;
  AddDestructionObserver(opener_);
}

void TestWebContents::AddPendingContents(TestWebContents* contents) {
  // This is normally only done in WebContentsImpl::CreateNewWindow.
  pending_contents_[contents->GetRenderViewHost()->GetRoutingID()] = contents;
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
  FrameHostMsg_DidFailLoadWithError msg(
      0, url, error_code, error_description);
  frame_tree_.root()->current_frame_host()->OnMessageReceived(msg);
}

void TestWebContents::CreateNewWindow(
    int render_process_id,
    int route_id,
    int main_frame_route_id,
    const ViewHostMsg_CreateWindow_Params& params,
    SessionStorageNamespace* session_storage_namespace) {
}

void TestWebContents::CreateNewWidget(int render_process_id,
                                      int route_id,
                                      blink::WebPopupType popup_type) {
}

void TestWebContents::CreateNewFullscreenWidget(int render_process_id,
                                                int route_id) {
}

void TestWebContents::ShowCreatedWindow(int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_rect,
                                        bool user_gesture) {
}

void TestWebContents::ShowCreatedWidget(int route_id,
                                        const gfx::Rect& initial_rect) {
}

void TestWebContents::ShowCreatedFullscreenWidget(int route_id) {
}

}  // namespace content

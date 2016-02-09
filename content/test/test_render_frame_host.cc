// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "content/test/browser_side_navigation_test_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_view_host.h"
#include "net/base/load_flags.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "ui/base/page_transition_types.h"

namespace content {

TestRenderFrameHostCreationObserver::TestRenderFrameHostCreationObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents), last_created_frame_(NULL) {
}

TestRenderFrameHostCreationObserver::~TestRenderFrameHostCreationObserver() {
}

void TestRenderFrameHostCreationObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  last_created_frame_ = render_frame_host;
}

TestRenderFrameHost::TestRenderFrameHost(SiteInstance* site_instance,
                                         RenderViewHostImpl* render_view_host,
                                         RenderFrameHostDelegate* delegate,
                                         RenderWidgetHostDelegate* rwh_delegate,
                                         FrameTree* frame_tree,
                                         FrameTreeNode* frame_tree_node,
                                         int32_t routing_id,
                                         int32_t widget_routing_id,
                                         int flags)
    : RenderFrameHostImpl(site_instance,
                          render_view_host,
                          delegate,
                          rwh_delegate,
                          frame_tree,
                          frame_tree_node,
                          routing_id,
                          widget_routing_id,
                          flags),
      child_creation_observer_(delegate ? delegate->GetAsWebContents() : NULL),
      contents_mime_type_("text/html"),
      simulate_history_list_was_cleared_(false) {}

TestRenderFrameHost::~TestRenderFrameHost() {
}

TestRenderViewHost* TestRenderFrameHost::GetRenderViewHost() {
  return static_cast<TestRenderViewHost*>(
      RenderFrameHostImpl::GetRenderViewHost());
}

MockRenderProcessHost* TestRenderFrameHost::GetProcess() {
  return static_cast<MockRenderProcessHost*>(RenderFrameHostImpl::GetProcess());
}

void TestRenderFrameHost::InitializeRenderFrameIfNeeded() {
  if (!render_view_host()->IsRenderViewLive()) {
    RenderViewHostTester::For(render_view_host())->CreateTestRenderView(
        base::string16(), MSG_ROUTING_NONE, MSG_ROUTING_NONE, -1, false);
  }
}

TestRenderFrameHost* TestRenderFrameHost::AppendChild(
    const std::string& frame_name) {
  OnCreateChildFrame(GetProcess()->GetNextRoutingID(),
                     blink::WebTreeScopeType::Document, frame_name,
                     blink::WebSandboxFlags::None,
                     blink::WebFrameOwnerProperties());
  return static_cast<TestRenderFrameHost*>(
      child_creation_observer_.last_created_frame());
}

void TestRenderFrameHost::SimulateNavigationStart(const GURL& url) {
  if (IsBrowserSideNavigationEnabled()) {
    SendRendererInitiatedNavigationRequest(url, false);
    return;
  }

  OnDidStartLoading(true);
  OnDidStartProvisionalLoad(url, base::TimeTicks::Now());
}

void TestRenderFrameHost::SimulateRedirect(const GURL& new_url) {
  if (IsBrowserSideNavigationEnabled()) {
    NavigationRequest* request = frame_tree_node_->navigation_request();
    TestNavigationURLLoader* url_loader =
        static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
    CHECK(url_loader);
    url_loader->SimulateServerRedirect(new_url);
    return;
  }

  navigation_handle()->CallWillRedirectRequestForTesting(new_url, false, GURL(),
                                                         false);
}

void TestRenderFrameHost::SimulateNavigationCommit(const GURL& url) {
  if (frame_tree_node()->navigation_request())
    PrepareForCommit();

  bool is_auto_subframe =
      GetParent() && !frame_tree_node()->has_committed_real_load();

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.page_id = ComputeNextPageID();
  params.nav_entry_id = 0;
  params.url = url;
  if (!GetParent())
    params.transition = ui::PAGE_TRANSITION_LINK;
  else if (is_auto_subframe)
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  else
    params.transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = true;
  params.did_create_new_entry = !is_auto_subframe;
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.is_post = false;
  params.http_status_code = 200;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = url;

  url::Replacements<char> replacements;
  replacements.ClearRef();
  params.was_within_same_page =
      url.ReplaceComponents(replacements) ==
      GetLastCommittedURL().ReplaceComponents(replacements);

  params.page_state = PageState::CreateForTesting(url, false, nullptr, nullptr);

  SendNavigateWithParams(&params);
}

void TestRenderFrameHost::SimulateNavigationError(const GURL& url,
                                                  int error_code) {
  if (IsBrowserSideNavigationEnabled()) {
    NavigationRequest* request = frame_tree_node_->navigation_request();
    CHECK(request);
    // Simulate a beforeUnload ACK from the renderer if the browser is waiting
    // for it. If it runs it will update the request state.
    if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
      static_cast<TestRenderFrameHost*>(frame_tree_node()->current_frame_host())
          ->SendBeforeUnloadACK(true);
    }
    TestNavigationURLLoader* url_loader =
        static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
    CHECK(url_loader);
    url_loader->SimulateError(error_code);
    return;
  }

  FrameHostMsg_DidFailProvisionalLoadWithError_Params error_params;
  error_params.error_code = error_code;
  error_params.url = url;
  OnDidFailProvisionalLoadWithError(error_params);
}

void TestRenderFrameHost::SimulateNavigationErrorPageCommit() {
  CHECK(navigation_handle());
  GURL error_url = GURL(kUnreachableWebDataURL);
  OnDidStartProvisionalLoad(error_url, base::TimeTicks::Now());
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.page_id = ComputeNextPageID();
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = navigation_handle()->GetURL();
  params.transition = GetParent() ? ui::PAGE_TRANSITION_MANUAL_SUBFRAME
                                  : ui::PAGE_TRANSITION_LINK;
  params.was_within_same_page = false;
  params.url_is_unreachable = true;
  params.page_state = PageState::CreateForTesting(navigation_handle()->GetURL(),
                                                  false, nullptr, nullptr);
  SendNavigateWithParams(&params);
}

void TestRenderFrameHost::SimulateNavigationStop() {
  if (is_loading()) {
    OnDidStopLoading();
  } else if (IsBrowserSideNavigationEnabled()) {
    // Even if the RenderFrameHost is not loading, there may still be an
    // ongoing navigation in the FrameTreeNode. Cancel this one as well.
    frame_tree_node()->ResetNavigationRequest(false);
  }
}

void TestRenderFrameHost::SetContentsMimeType(const std::string& mime_type) {
  contents_mime_type_ = mime_type;
}

void TestRenderFrameHost::SendBeforeUnloadACK(bool proceed) {
  base::TimeTicks now = base::TimeTicks::Now();
  OnBeforeUnloadACK(proceed, now, now);
}

void TestRenderFrameHost::SimulateSwapOutACK() {
  OnSwappedOut();
}

void TestRenderFrameHost::SendNavigate(int page_id,
                                       int nav_entry_id,
                                       bool did_create_new_entry,
                                       const GURL& url) {
  SendNavigateWithParameters(page_id, nav_entry_id, did_create_new_entry, false,
                             url, ui::PAGE_TRANSITION_LINK, 200,
                             ModificationCallback());
}

void TestRenderFrameHost::SendFailedNavigate(int page_id,
                                             int nav_entry_id,
                                             bool did_create_new_entry,
                                             const GURL& url) {
  SendNavigateWithParameters(page_id, nav_entry_id, did_create_new_entry, false,
                             url, ui::PAGE_TRANSITION_RELOAD, 500,
                             ModificationCallback());
}

void TestRenderFrameHost::SendNavigateWithTransition(
    int page_id,
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    ui::PageTransition transition) {
  SendNavigateWithParameters(page_id, nav_entry_id, did_create_new_entry, false,
                             url, transition, 200, ModificationCallback());
}

void TestRenderFrameHost::SendNavigateWithReplacement(int page_id,
                                                      int nav_entry_id,
                                                      bool did_create_new_entry,
                                                      const GURL& url) {
  SendNavigateWithParameters(page_id, nav_entry_id, did_create_new_entry, true,
                             url, ui::PAGE_TRANSITION_LINK, 200,
                             ModificationCallback());
}

void TestRenderFrameHost::SendNavigateWithModificationCallback(
    int page_id,
    int nav_entry_id,
    bool did_create_new_entry,
    const GURL& url,
    const ModificationCallback& callback) {
  SendNavigateWithParameters(page_id, nav_entry_id, did_create_new_entry, false,
                             url, ui::PAGE_TRANSITION_LINK, 200, callback);
}

void TestRenderFrameHost::SendNavigateWithParameters(
    int page_id,
    int nav_entry_id,
    bool did_create_new_entry,
    bool should_replace_entry,
    const GURL& url,
    ui::PageTransition transition,
    int response_code,
    const ModificationCallback& callback) {
  // DidStartProvisionalLoad may delete the pending entry that holds |url|,
  // so we keep a copy of it to use below.
  GURL url_copy(url);
  OnDidStartLoading(true);
  OnDidStartProvisionalLoad(url_copy, base::TimeTicks::Now());

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.page_id = page_id;
  params.nav_entry_id = nav_entry_id;
  params.url = url_copy;
  params.transition = transition;
  params.should_update_history = true;
  params.did_create_new_entry = did_create_new_entry;
  params.should_replace_current_entry = should_replace_entry;
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.is_post = false;
  params.http_status_code = response_code;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = url_copy;

  // In most cases, the origin will match the URL's origin.  Tests that need to
  // check corner cases (like about:blank) should specify the origin param
  // manually.
  url::Origin origin(url_copy);
  params.origin = origin;

  url::Replacements<char> replacements;
  replacements.ClearRef();
  params.was_within_same_page =
      transition != ui::PAGE_TRANSITION_RELOAD &&
      transition != ui::PAGE_TRANSITION_TYPED &&
      url_copy.ReplaceComponents(replacements) ==
          GetLastCommittedURL().ReplaceComponents(replacements);

  params.page_state =
      PageState::CreateForTesting(url_copy, false, nullptr, nullptr);

  if (!callback.is_null())
    callback.Run(&params);

  SendNavigateWithParams(&params);
}

void TestRenderFrameHost::SendNavigateWithParams(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  FrameHostMsg_DidCommitProvisionalLoad msg(GetRoutingID(), *params);
  OnDidCommitProvisionalLoad(msg);
}

void TestRenderFrameHost::NavigateAndCommitRendererInitiated(
    int page_id,
    bool did_create_new_entry,
    const GURL& url) {
  SendRendererInitiatedNavigationRequest(url, false);
  // PlzNavigate: If no network request is needed by the navigation, then there
  // will be no NavigationRequest, nor is it necessary to simulate the network
  // stack commit.
  if (frame_tree_node()->navigation_request())
    PrepareForCommit();
  bool browser_side_navigation = IsBrowserSideNavigationEnabled();
  CHECK(!browser_side_navigation || is_loading());
  CHECK(!browser_side_navigation || !frame_tree_node()->navigation_request());
  SendNavigate(page_id, 0, did_create_new_entry, url);
}

void TestRenderFrameHost::SendRendererInitiatedNavigationRequest(
    const GURL& url,
    bool has_user_gesture) {
  // Since this is renderer-initiated navigation, the RenderFrame must be
  // initialized. Do it if it hasn't happened yet.
  InitializeRenderFrameIfNeeded();

  if (IsBrowserSideNavigationEnabled()) {
    BeginNavigationParams begin_params("GET", std::string(), net::LOAD_NORMAL,
                                       has_user_gesture, false,
                                       REQUEST_CONTEXT_TYPE_HYPERLINK);
    CommonNavigationParams common_params;
    common_params.url = url;
    common_params.referrer = Referrer(GURL(), blink::WebReferrerPolicyDefault);
    common_params.transition = ui::PAGE_TRANSITION_LINK;
    OnBeginNavigation(common_params, begin_params,
                      scoped_refptr<ResourceRequestBody>());
  }
}

void TestRenderFrameHost::DidChangeOpener(int opener_routing_id) {
  OnDidChangeOpener(opener_routing_id);
}

void TestRenderFrameHost::DidEnforceStrictMixedContentChecking() {
  OnEnforceStrictMixedContentChecking();
}

void TestRenderFrameHost::PrepareForCommit() {
  PrepareForCommitWithServerRedirect(GURL());
}

void TestRenderFrameHost::PrepareForCommitWithServerRedirect(
    const GURL& redirect_url) {
  if (!IsBrowserSideNavigationEnabled()) {
    // Non PlzNavigate
    if (is_waiting_for_beforeunload_ack())
      SendBeforeUnloadACK(true);
    return;
  }

  // PlzNavigate
  NavigationRequest* request = frame_tree_node_->navigation_request();
  CHECK(request);

  // Simulate a beforeUnload ACK from the renderer if the browser is waiting for
  // it. If it runs it will update the request state.
  if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
    static_cast<TestRenderFrameHost*>(frame_tree_node()->current_frame_host())
        ->SendBeforeUnloadACK(true);
  }

  CHECK(request->state() == NavigationRequest::STARTED);

  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  CHECK(url_loader);

  // If a non-empty |redirect_url| was provided, simulate a server redirect.
  if (!redirect_url.is_empty())
    url_loader->SimulateServerRedirect(redirect_url);

  // Simulate the network stack commit.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  // TODO(carlosk): ideally with PlzNavigate it should be possible someday to
  // fully commit the navigation at this call to CallOnResponseStarted.
  url_loader->CallOnResponseStarted(response, MakeEmptyStream());
}

int32_t TestRenderFrameHost::ComputeNextPageID() {
  const NavigationEntryImpl* entry = static_cast<NavigationEntryImpl*>(
      frame_tree_node()->navigator()->GetController()->GetPendingEntry());
  DCHECK(!(entry && entry->site_instance()) ||
         entry->site_instance() == GetSiteInstance());
  // Entry can be null when committing an error page (the pending entry was
  // cleared during DidFailProvisionalLoad).
  int page_id = entry ? entry->GetPageID() : -1;
  if (page_id == -1) {
    WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(delegate());
    page_id = web_contents->GetMaxPageIDForSiteInstance(GetSiteInstance()) + 1;
  }
  return page_id;
}

}  // namespace content

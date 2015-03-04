// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include "base/command_line.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_switches.h"
#include "content/test/browser_side_navigation_test_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_view_host.h"
#include "net/base/load_flags.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
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
                                         int routing_id,
                                         int flags)
    : RenderFrameHostImpl(site_instance,
                          render_view_host,
                          delegate,
                          rwh_delegate,
                          frame_tree,
                          frame_tree_node,
                          routing_id,
                          flags),
      child_creation_observer_(delegate ? delegate->GetAsWebContents() : NULL),
      contents_mime_type_("text/html"),
      simulate_history_list_was_cleared_(false) {
  if (frame_tree_node_->IsMainFrame())
    SetRenderFrameCreated(true);
}

TestRenderFrameHost::~TestRenderFrameHost() {
  SetRenderFrameCreated(false);
}

TestRenderViewHost* TestRenderFrameHost::GetRenderViewHost() {
  return static_cast<TestRenderViewHost*>(
      RenderFrameHostImpl::GetRenderViewHost());
}

TestRenderFrameHost* TestRenderFrameHost::AppendChild(
    const std::string& frame_name) {
  OnCreateChildFrame(GetProcess()->GetNextRoutingID(), frame_name,
                     SandboxFlags::NONE);
  return static_cast<TestRenderFrameHost*>(
      child_creation_observer_.last_created_frame());
}

void TestRenderFrameHost::SendNavigateWithTransition(
    int page_id,
    const GURL& url,
    ui::PageTransition transition) {
  SendNavigateWithTransitionAndResponseCode(page_id, url, transition, 200);
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

void TestRenderFrameHost::SendNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransition(page_id, url, ui::PAGE_TRANSITION_LINK);
}

void TestRenderFrameHost::SendFailedNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransitionAndResponseCode(
      page_id, url, ui::PAGE_TRANSITION_RELOAD, 500);
}

void TestRenderFrameHost::SendNavigateWithTransitionAndResponseCode(
    int page_id,
    const GURL& url, ui::PageTransition transition,
    int response_code) {
  // DidStartProvisionalLoad may delete the pending entry that holds |url|,
  // so we keep a copy of it to use in SendNavigateWithParameters.
  GURL url_copy(url);

  // Ensure that the RenderFrameCreated notification has been sent to observers
  // before navigating the frame.
  SetRenderFrameCreated(true);

  OnDidStartProvisionalLoadForFrame(url_copy, false);
  SendNavigateWithParameters(page_id, url_copy, transition, url_copy,
      response_code, 0, std::vector<GURL>());
}

void TestRenderFrameHost::SendNavigateWithOriginalRequestURL(
    int page_id,
    const GURL& url,
    const GURL& original_request_url) {
  // Ensure that the RenderFrameCreated notification has been sent to observers
  // before navigating the frame.
  SetRenderFrameCreated(true);

  OnDidStartProvisionalLoadForFrame(url, false);
  SendNavigateWithParameters(page_id, url, ui::PAGE_TRANSITION_LINK,
      original_request_url, 200, 0, std::vector<GURL>());
}

void TestRenderFrameHost::SendNavigateWithFile(
    int page_id,
    const GURL& url,
    const base::FilePath& file_path) {
  SendNavigateWithParameters(page_id, url, ui::PAGE_TRANSITION_LINK, url, 200,
      &file_path, std::vector<GURL>());
}

void TestRenderFrameHost::SendNavigateWithParams(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  FrameHostMsg_DidCommitProvisionalLoad msg(GetRoutingID(), *params);
  OnDidCommitProvisionalLoad(msg);
}

void TestRenderFrameHost::SendNavigateWithRedirects(
    int page_id,
    const GURL& url,
    const std::vector<GURL>& redirects) {
  SendNavigateWithParameters(
      page_id, url, ui::PAGE_TRANSITION_LINK, url, 200, 0, redirects);
}

void TestRenderFrameHost::SendNavigateWithParameters(
    int page_id,
    const GURL& url,
    ui::PageTransition transition,
    const GURL& original_request_url,
    int response_code,
    const base::FilePath* file_path_for_history_item,
    const std::vector<GURL>& redirects) {
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.page_id = page_id;
  params.url = url;
  params.referrer = Referrer();
  params.transition = transition;
  params.redirects = redirects;
  params.should_update_history = true;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.is_post = false;
  params.http_status_code = response_code;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = original_request_url;

  url::Replacements<char> replacements;
  replacements.ClearRef();
  params.was_within_same_page = transition != ui::PAGE_TRANSITION_RELOAD &&
      transition != ui::PAGE_TRANSITION_TYPED &&
      url.ReplaceComponents(replacements) ==
          GetLastCommittedURL().ReplaceComponents(replacements);

  params.page_state = PageState::CreateForTesting(
      url,
      false,
      file_path_for_history_item ? "data" : NULL,
      file_path_for_history_item);

  FrameHostMsg_DidCommitProvisionalLoad msg(GetRoutingID(), params);
  OnDidCommitProvisionalLoad(msg);
}

void TestRenderFrameHost::SendRendererInitiatedNavigationRequest(
    const GURL& url,
    bool has_user_gesture) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    BeginNavigationParams begin_params("GET", std::string(), net::LOAD_NORMAL,
                                       has_user_gesture);
    CommonNavigationParams common_params;
    common_params.url = url;
    common_params.referrer = Referrer(GURL(), blink::WebReferrerPolicyDefault);
    common_params.transition = ui::PAGE_TRANSITION_LINK;
    OnBeginNavigation(common_params, begin_params,
                      scoped_refptr<ResourceRequestBody>());
  }
}

void TestRenderFrameHost::DidDisownOpener() {
  OnDidDisownOpener();
}

void TestRenderFrameHost::PrepareForCommit() {
  PrepareForCommitWithServerRedirect(GURL());
}

void TestRenderFrameHost::PrepareForCommitWithServerRedirect(
    const GURL& redirect_url) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    // Non PlzNavigate
    SendBeforeUnloadACK(true);
    return;
  }

  // PlzNavigate
  NavigationRequest* request =
      static_cast<NavigatorImpl*>(frame_tree_node_->navigator())
          ->GetNavigationRequestForNodeForTesting(frame_tree_node_);
  CHECK(request);

  // Simulate a beforeUnload ACK from the renderer if the browser is waiting for
  // it. If it runs it will update the request state.
  if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE)
    SendBeforeUnloadACK(true);

  // If a network request is not needed for this URL, just check the request is
  // in the correct state and return.
  if (!request->ShouldMakeNetworkRequest(request->common_params().url)) {
    CHECK(request->state() == NavigationRequest::RESPONSE_STARTED);
    return;
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

void TestRenderFrameHost::SendBeforeUnloadHandlersPresent(bool present) {
  OnBeforeUnloadHandlersPresent(present);
}

void TestRenderFrameHost::SendUnloadHandlersPresent(bool present) {
  OnUnloadHandlersPresent(present);
}

}  // namespace content

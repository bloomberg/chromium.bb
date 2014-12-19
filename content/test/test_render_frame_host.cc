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
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_switches.h"
#include "content/test/browser_side_navigation_test_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_view_host.h"
#include "net/base/load_flags.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"
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

TestRenderFrameHost::TestRenderFrameHost(RenderViewHostImpl* render_view_host,
                                         RenderFrameHostDelegate* delegate,
                                         FrameTree* frame_tree,
                                         FrameTreeNode* frame_tree_node,
                                         int routing_id,
                                         int flags)
    : RenderFrameHostImpl(render_view_host,
                          delegate,
                          frame_tree,
                          frame_tree_node,
                          routing_id,
                          flags),
      child_creation_observer_(delegate ? delegate->GetAsWebContents() : NULL),
      contents_mime_type_("text/html"),
      simulate_history_list_was_cleared_(false) {
}

TestRenderFrameHost::~TestRenderFrameHost() {}

TestRenderViewHost* TestRenderFrameHost::GetRenderViewHost() {
  return static_cast<TestRenderViewHost*>(
      RenderFrameHostImpl::GetRenderViewHost());
}

TestRenderFrameHost* TestRenderFrameHost::AppendChild(
    const std::string& frame_name) {
  OnCreateChildFrame(GetProcess()->GetNextRoutingID(), frame_name);
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
  OnDidStartProvisionalLoadForFrame(url_copy, false);
  SendNavigateWithParameters(page_id, url_copy, transition, url_copy,
      response_code, 0, std::vector<GURL>());
}

void TestRenderFrameHost::SendNavigateWithOriginalRequestURL(
    int page_id,
    const GURL& url,
    const GURL& original_request_url) {
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

void TestRenderFrameHost::SendBeginNavigationWithURL(const GURL& url) {
  FrameHostMsg_BeginNavigation_Params begin_params;
  CommonNavigationParams common_params;
  begin_params.method = "GET";
  begin_params.load_flags = net::LOAD_NORMAL;
  begin_params.has_user_gesture = false;
  common_params.url = url;
  common_params.referrer = Referrer(GURL(), blink::WebReferrerPolicyDefault);
  common_params.transition = ui::PAGE_TRANSITION_LINK;
  OnBeginNavigation(begin_params, common_params);
}

void TestRenderFrameHost::DidDisownOpener() {
  OnDidDisownOpener();
}

void TestRenderFrameHost::PrepareForCommit(const GURL& url) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    SendBeforeUnloadACK(true);
    return;
  }

  // PlzNavigate
  // Simulate the network stack commit without any redirects.
  NavigationRequest* request =
      static_cast<NavigatorImpl*>(frame_tree_node_->navigator())
          ->GetNavigationRequestForNodeForTesting(frame_tree_node_);

  // We are simulating a renderer-initiated navigation.
  if (!request) {
    SendBeginNavigationWithURL(url);
    request = static_cast<NavigatorImpl*>(frame_tree_node_->navigator())
        ->GetNavigationRequestForNodeForTesting(frame_tree_node_);
  }
  ASSERT_TRUE(request);

  // We may not have simulated the renderer response to the navigation request.
  // Do that now.
  if (request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE)
    SendBeginNavigationWithURL(url);

  // We have already simulated the IO thread commit. Only the
  // DidCommitProvisionalLoad from the renderer is missing.
  if (request->state() == NavigationRequest::RESPONSE_STARTED)
    return;

  ASSERT_TRUE(request->state() == NavigationRequest::STARTED);
  TestNavigationURLLoader* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  ASSERT_TRUE(url_loader);
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  url_loader->CallOnResponseStarted(response, MakeEmptyStream());
}

}  // namespace content

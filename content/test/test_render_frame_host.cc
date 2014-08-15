// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/common/frame_messages.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/test_render_view_host.h"
#include "net/base/load_flags.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"

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
                                         bool is_swapped_out)
    : RenderFrameHostImpl(render_view_host,
                          delegate,
                          frame_tree,
                          frame_tree_node,
                          routing_id,
                          is_swapped_out),
      child_creation_observer_(delegate ? delegate->GetAsWebContents() : NULL),
      contents_mime_type_("text/html"),
      simulate_history_list_was_cleared_(false) {
  // Allow TestRenderViewHosts to easily access their main frame RFH.
  if (frame_tree_node == frame_tree->root()) {
    static_cast<TestRenderViewHost*>(render_view_host)->
        set_main_render_frame_host(this);
  }
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
    PageTransition transition) {
  SendNavigateWithTransitionAndResponseCode(page_id, url, transition, 200);
}

void TestRenderFrameHost::SendNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransition(page_id, url, PAGE_TRANSITION_LINK);
}

void TestRenderFrameHost::SendFailedNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransitionAndResponseCode(
      page_id, url, PAGE_TRANSITION_RELOAD, 500);
}

void TestRenderFrameHost::SendNavigateWithTransitionAndResponseCode(
    int page_id,
    const GURL& url, PageTransition transition,
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
  SendNavigateWithParameters(page_id, url, PAGE_TRANSITION_LINK,
      original_request_url, 200, 0, std::vector<GURL>());
}

void TestRenderFrameHost::SendNavigateWithFile(
    int page_id,
    const GURL& url,
    const base::FilePath& file_path) {
  SendNavigateWithParameters(page_id, url, PAGE_TRANSITION_LINK, url, 200,
      &file_path, std::vector<GURL>());
}

void TestRenderFrameHost::SendNavigateWithParams(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  FrameHostMsg_DidCommitProvisionalLoad msg(GetRoutingID(), *params);
  OnNavigate(msg);
}

void TestRenderFrameHost::SendNavigateWithRedirects(
    int page_id,
    const GURL& url,
    const std::vector<GURL>& redirects) {
  SendNavigateWithParameters(
      page_id, url, PAGE_TRANSITION_LINK, url, 200, 0, redirects);
}

void TestRenderFrameHost::SendNavigateWithParameters(
    int page_id,
    const GURL& url,
    PageTransition transition,
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
  params.was_within_same_page = transition != PAGE_TRANSITION_RELOAD &&
      transition != PAGE_TRANSITION_TYPED &&
      url.ReplaceComponents(replacements) ==
          GetLastCommittedURL().ReplaceComponents(replacements);

  params.page_state = PageState::CreateForTesting(
      url,
      false,
      file_path_for_history_item ? "data" : NULL,
      file_path_for_history_item);

  FrameHostMsg_DidCommitProvisionalLoad msg(GetRoutingID(), params);
  OnNavigate(msg);
}

void TestRenderFrameHost::SendBeginNavigationWithURL(const GURL& url) {
  FrameHostMsg_BeginNavigation_Params params;
  params.method = "GET";
  params.url = url;
  params.referrer_policy = blink::WebReferrerPolicyDefault;
  params.load_flags = net::LOAD_NORMAL;
  params.has_user_gesture = false;
  params.transition_type = PAGE_TRANSITION_LINK;
  params.should_replace_current_entry = false;
  params.allow_download = true;
  // TODO(clamy): When the BeginNavigation handler is no longer compiled out,
  // call OnBeginNavigation directly.
  frame_tree_node()->render_manager()->OnBeginNavigation(params);
}
}  // namespace content

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include "content/browser/frame_host/frame_tree.h"
#include "content/common/frame_messages.h"
#include "content/test/test_render_view_host.h"

namespace {

const int64 kFrameId = 13UL;

}  // namespace

namespace content {

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
      contents_mime_type_("text/html"),
      simulate_history_list_was_cleared_(false) {
  // Allow TestRenderViewHosts to easily access their main frame RFH.
  if (frame_tree_node == frame_tree->root()) {
    static_cast<TestRenderViewHost*>(render_view_host)->
        set_main_render_frame_host(this);
  }
}

TestRenderFrameHost::~TestRenderFrameHost() {}

void TestRenderFrameHost::SendNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransition(page_id, url, PAGE_TRANSITION_LINK);
}

void TestRenderFrameHost::SendNavigateWithTransition(
    int page_id,
    const GURL& url,
    PageTransition transition) {
  SendNavigateWithTransitionAndResponseCode(page_id, url, transition, 200);
}

void TestRenderFrameHost::SendFailedNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransitionAndResponseCode(
      page_id, url, PAGE_TRANSITION_LINK, 500);
}

void TestRenderFrameHost::SendNavigateWithTransitionAndResponseCode(
    int page_id,
    const GURL& url, PageTransition transition,
    int response_code) {
  // DidStartProvisionalLoad may delete the pending entry that holds |url|,
  // so we keep a copy of it to use in SendNavigateWithParameters.
  GURL url_copy(url);
  OnDidStartProvisionalLoadForFrame(kFrameId, -1, true, url_copy);
  SendNavigateWithParameters(
      page_id, url_copy, transition, url_copy, response_code, 0);
}

void TestRenderFrameHost::SendNavigateWithOriginalRequestURL(
    int page_id,
    const GURL& url,
    const GURL& original_request_url) {
  OnDidStartProvisionalLoadForFrame(kFrameId, -1, true, url);
  SendNavigateWithParameters(
      page_id, url, PAGE_TRANSITION_LINK, original_request_url, 200, 0);
}

void TestRenderFrameHost::SendNavigateWithFile(
    int page_id,
    const GURL& url,
    const base::FilePath& file_path) {
  SendNavigateWithParameters(
      page_id, url, PAGE_TRANSITION_LINK, url, 200, &file_path);
}

void TestRenderFrameHost::SendNavigateWithParams(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  params->frame_id = kFrameId;
  FrameHostMsg_DidCommitProvisionalLoad msg(1, *params);
  OnNavigate(msg);
}

void TestRenderFrameHost::SendNavigateWithParameters(
    int page_id,
    const GURL& url,
    PageTransition transition,
    const GURL& original_request_url,
    int response_code,
    const base::FilePath* file_path_for_history_item) {
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.page_id = page_id;
  params.frame_id = kFrameId;
  params.url = url;
  params.referrer = Referrer();
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = true;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.is_post = false;
  params.was_within_same_page = false;
  params.http_status_code = response_code;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = original_request_url;

  params.page_state = PageState::CreateForTesting(
      url,
      false,
      file_path_for_history_item ? "data" : NULL,
      file_path_for_history_item);

  FrameHostMsg_DidCommitProvisionalLoad msg(1, params);
  OnNavigate(msg);
}

}  // namespace content

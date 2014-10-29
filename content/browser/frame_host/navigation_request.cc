// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/common/resource_request_body.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/stream_handle.h"
#include "net/url_request/redirect_info.h"

namespace content {

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params)
    : frame_tree_node_(frame_tree_node),
      common_params_(common_params),
      commit_params_(commit_params) {
}

NavigationRequest::~NavigationRequest() {
}

void NavigationRequest::BeginNavigation(
    scoped_ptr<NavigationRequestInfo> info,
    scoped_refptr<ResourceRequestBody> request_body) {
  DCHECK(!loader_);
  loader_ = NavigationURLLoader::Create(
      frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
      frame_tree_node_->frame_tree_node_id(), common_params_, info.Pass(),
      request_body.get(), this);

  // TODO(davidben): Fire (and add as necessary) observer methods such as
  // DidStartProvisionalLoadForFrame for the navigation.
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  // TODO(davidben): Track other changes from redirects. These are important
  // for, e.g., reloads.
  common_params_.url = redirect_info.new_url;

  // TODO(davidben): This where prerender and navigation_interceptor should be
  // integrated. For now, just always follow all redirects.
  loader_->FollowRedirect();
}

void NavigationRequest::OnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    scoped_ptr<StreamHandle> body) {
  frame_tree_node_->navigator()->CommitNavigation(frame_tree_node_,
                                                  response.get(), body.Pass());
}

void NavigationRequest::OnRequestFailed(int net_error) {
  // TODO(davidben): Network failures should display a network error page.
  NOTIMPLEMENTED();
}

}  // namespace content

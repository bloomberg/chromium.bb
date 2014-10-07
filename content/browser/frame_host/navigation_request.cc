// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include "content/browser/frame_host/navigation_request_info.h"
#include "content/common/resource_request_body.h"

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
  info_ = info.Pass();
  NOTIMPLEMENTED();
}

}  // namespace content

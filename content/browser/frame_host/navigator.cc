// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator.h"

#include "base/time/time.h"
#include "content/common/resource_request_body.h"
#include "content/public/browser/stream_handle.h"

namespace content {

NavigationController* Navigator::GetController() {
  return NULL;
}

bool Navigator::NavigateToPendingEntry(
    FrameTreeNode* frame_tree_node,
    NavigationController::ReloadType reload_type) {
  return false;
}

base::TimeTicks Navigator::GetCurrentLoadStart() {
  return base::TimeTicks::Now();
}

void Navigator::OnBeginNavigation(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    scoped_refptr<ResourceRequestBody> body) {
}

void Navigator::CommitNavigation(FrameTreeNode* frame_tree_node,
                                 ResourceResponse* response,
                                 scoped_ptr<StreamHandle> body) {
}

bool Navigator::IsWaitingForBeforeUnloadACK(FrameTreeNode* frame_tree_node) {
  return false;
}

}  // namespace content

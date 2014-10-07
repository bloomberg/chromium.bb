// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator.h"

#include "base/time/time.h"
#include "content/public/browser/stream_handle.h"

namespace content {

NavigationController* Navigator::GetController() {
  return NULL;
}

bool Navigator::NavigateToPendingEntry(
    RenderFrameHostImpl* render_frame_host,
    NavigationController::ReloadType reload_type) {
  return false;
}

base::TimeTicks Navigator::GetCurrentLoadStart() {
  return base::TimeTicks::Now();
}

void Navigator::CommitNavigation(FrameTreeNode* frame_tree_node,
                                 ResourceResponse* response,
                                 scoped_ptr<StreamHandle> body) {
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator.h"

#include "base/time/time.h"
#include "content/public/browser/stream_handle.h"

namespace content {

NavigatorDelegate* Navigator::GetDelegate() {
  return nullptr;
}

NavigationController* Navigator::GetController() {
  return NULL;
}

bool Navigator::NavigateToPendingEntry(FrameTreeNode* frame_tree_node,
                                       const FrameNavigationEntry& frame_entry,
                                       ReloadType reload_type,
                                       bool is_same_document_history_load) {
  return false;
}

bool Navigator::NavigateNewChildFrame(RenderFrameHostImpl* render_frame_host,
                                      const GURL& default_url) {
  return false;
}

base::TimeTicks Navigator::GetCurrentLoadStart() {
  return base::TimeTicks::Now();
}

void Navigator::OnBeginNavigation(FrameTreeNode* frame_tree_node,
                                  const CommonNavigationParams& common_params,
                                  const BeginNavigationParams& begin_params) {}

}  // namespace content

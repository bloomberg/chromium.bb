// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils_internal.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/test/test_frame_navigation_observer.h"
#include "url/gurl.h"

namespace content {

void NavigateFrameToURL(FrameTreeNode* node, const GURL& url) {
  TestFrameNavigationObserver observer(node);
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = node->frame_tree_node_id();
  node->navigator()->GetController()->LoadURLWithParams(params);
  observer.Wait();
}

}  // namespace content

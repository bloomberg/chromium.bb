// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_frame_host.h"

#include "content/browser/frame_host/frame_tree.h"
#include "content/test/test_render_view_host.h"

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
                          is_swapped_out) {
  // Allow TestRenderViewHosts to easily access their main frame RFH.
  if (frame_tree_node == frame_tree->root()) {
    static_cast<TestRenderViewHost*>(render_view_host)->
        set_main_render_frame_host(this);
  }
}

TestRenderFrameHost::~TestRenderFrameHost() {}

}  // namespace content

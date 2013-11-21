// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/test_render_frame_host.h"

namespace content {

TestRenderFrameHost::TestRenderFrameHost(RenderViewHostImpl* render_view_host,
                                         FrameTree* frame_tree,
                                         int routing_id,
                                         bool is_swapped_out)
    : RenderFrameHostImpl(render_view_host,
                          frame_tree,
                          routing_id,
                          is_swapped_out) {}

TestRenderFrameHost::~TestRenderFrameHost() {}

}  // namespace content

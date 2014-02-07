// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_

#include "base/basictypes.h"
#include "content/browser/frame_host/render_frame_host_impl.h"

namespace content {

class TestRenderFrameHost : public RenderFrameHostImpl {
 public:
  TestRenderFrameHost(RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int routing_id,
                      bool is_swapped_out);
  virtual ~TestRenderFrameHost();

  // TODO(nick): As necessary for testing, override behavior of RenderFrameHost
  // here.

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameHost);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_

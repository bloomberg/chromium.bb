// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_HOST_FACTORY_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_HOST_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_frame_host_factory.h"

namespace content {

// Manages creation of the RenderFrameHostImpls; when registered, all created
// RenderFrameHostsImpls will be TestRenderFrameHosts. This
// automatically registers itself when it goes in scope, and unregisters itself
// when it goes out of scope. Since you can't have more than one factory
// registered at a time, you can only have one of these objects at a time.
class TestRenderFrameHostFactory : public RenderFrameHostFactory {
 public:
  TestRenderFrameHostFactory();
  virtual ~TestRenderFrameHostFactory();

 protected:
  // RenderFrameHostFactory implementation.
  virtual scoped_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
      RenderViewHostImpl* render_view_host,
      RenderFrameHostDelegate* delegate,
      FrameTree* frame_tree,
      FrameTreeNode* frame_tree_node,
      int routing_id,
      bool is_swapped_out) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameHostFactory);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_HOST_FACTORY_H_

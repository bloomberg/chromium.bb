// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_context.h"

#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"

namespace content {

class RenderFrameHost;

class WakeLockServiceContextTest : public RenderViewHostTestHarness {
 protected:
  void RequestWakeLock(RenderFrameHost* rfh) {
    GetWakeLockServiceContext()->RequestWakeLock(RFHToFrameNodeID(rfh));
  }

  void CancelWakeLock(RenderFrameHost* rfh) {
    GetWakeLockServiceContext()->CancelWakeLock(RFHToFrameNodeID(rfh));
  }

  int RFHToFrameNodeID(RenderFrameHost* rfh) {
    RenderFrameHostImpl* rfh_impl =
      static_cast<RenderFrameHostImpl*>(rfh);
    return rfh_impl->frame_tree_node()->frame_tree_node_id();
  }

  WakeLockServiceContext* GetWakeLockServiceContext() {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(web_contents());
    return web_contents_impl->GetWakeLockServiceContext();
  }

  bool HasWakeLock() {
    return GetWakeLockServiceContext()->HasWakeLockForTests();
  }
};

TEST_F(WakeLockServiceContextTest, NoLockInitially) {
  EXPECT_FALSE(HasWakeLock());
}

TEST_F(WakeLockServiceContextTest, LockUnlock) {
  ASSERT_TRUE(web_contents());
  ASSERT_TRUE(main_rfh());

  // Request wake lock for main frame
  RequestWakeLock(main_rfh());

  // Should set the blocker
  EXPECT_TRUE(HasWakeLock());

  // Remove wake lock request for main frame
  CancelWakeLock(main_rfh());

  // Should remove the blocker
  EXPECT_FALSE(HasWakeLock());
}

TEST_F(WakeLockServiceContextTest, RenderFrameDeleted) {
  ASSERT_TRUE(GetWakeLockServiceContext());
  ASSERT_TRUE(web_contents());
  ASSERT_TRUE(main_rfh());

  // Request wake lock for main frame
  RequestWakeLock(main_rfh());

  // Should set the blocker
  EXPECT_TRUE(HasWakeLock());

  // Simulate render frame deletion
  GetWakeLockServiceContext()->RenderFrameDeleted(main_rfh());

  // Should remove the blocker
  EXPECT_FALSE(HasWakeLock());
}

}  // namespace content

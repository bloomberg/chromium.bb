// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_
#define CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_

#include "cc/trees/layer_tree_host_client.h"

namespace cc {

class StubLayerTreeHostClient : public LayerTreeHostClient {
 public:
  ~StubLayerTreeHostClient() override;

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewCompositorFrameSink() override {}
  void DidInitializeCompositorFrameSink() override {}
  void DidFailToInitializeCompositorFrameSink() override {}
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override {}
  void DidCompletePageScaleAnimation() override {}
};

}  // namespace cc

#endif  // CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_

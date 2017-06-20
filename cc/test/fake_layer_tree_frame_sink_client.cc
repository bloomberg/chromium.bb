// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "cc/output/layer_tree_frame_sink.h"

namespace cc {

void FakeLayerTreeFrameSinkClient::SetBeginFrameSource(
    BeginFrameSource* source) {
  begin_frame_source_ = source;
}

void FakeLayerTreeFrameSinkClient::DidReceiveCompositorFrameAck() {
  ack_count_++;
}

void FakeLayerTreeFrameSinkClient::DidLoseLayerTreeFrameSink() {
  did_lose_layer_tree_frame_sink_called_ = true;
}

void FakeLayerTreeFrameSinkClient::SetMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  memory_policy_ = policy;
}

}  // namespace cc

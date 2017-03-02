// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame_sink.h"
#include "cc/test/fake_compositor_frame_sink_client.h"

namespace cc {

void FakeCompositorFrameSinkClient::SetBeginFrameSource(
    BeginFrameSource* source) {
  begin_frame_source_ = source;
}

void FakeCompositorFrameSinkClient::DidReceiveCompositorFrameAck() {
  ack_count_++;
}

void FakeCompositorFrameSinkClient::DidLoseCompositorFrameSink() {
  did_lose_compositor_frame_sink_called_ = true;
}

void FakeCompositorFrameSinkClient::SetMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  memory_policy_ = policy;
}

}  // namespace cc

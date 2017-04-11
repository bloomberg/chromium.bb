// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_renderer_compositor_frame_sink.h"

namespace content {

FakeRendererCompositorFrameSink::FakeRendererCompositorFrameSink(
    cc::mojom::MojoCompositorFrameSinkPtr sink,
    cc::mojom::MojoCompositorFrameSinkClientRequest request)
    : binding_(this, std::move(request)), sink_(std::move(sink)) {}

FakeRendererCompositorFrameSink::~FakeRendererCompositorFrameSink() = default;

void FakeRendererCompositorFrameSink::DidReceiveCompositorFrameAck(
    const cc::ReturnedResourceArray& resources) {
  ReclaimResources(resources);
  did_receive_ack_ = true;
}

void FakeRendererCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  last_reclaimed_resources_ = resources;
}

void FakeRendererCompositorFrameSink::Reset() {
  did_receive_ack_ = false;
  last_reclaimed_resources_.clear();
}

void FakeRendererCompositorFrameSink::Flush() {
  binding_.FlushForTesting();
}

}  // namespace content

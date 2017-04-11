// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_RENDERER_COMPOSITOR_FRAME_SINK_H_
#define CONTENT_TEST_FAKE_RENDERER_COMPOSITOR_FRAME_SINK_H_

#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

// This class is given to RenderWidgetHost/RenderWidgetHostView unit tests
// instead of RendererCompositorFrameSink.
class FakeRendererCompositorFrameSink
    : public cc::mojom::MojoCompositorFrameSinkClient {
 public:
  FakeRendererCompositorFrameSink(
      cc::mojom::MojoCompositorFrameSinkPtr sink,
      cc::mojom::MojoCompositorFrameSinkClientRequest request);
  ~FakeRendererCompositorFrameSink() override;

  bool did_receive_ack() { return did_receive_ack_; }
  cc::ReturnedResourceArray& last_reclaimed_resources() {
    return last_reclaimed_resources_;
  }

  // cc::mojom::MojoCompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const cc::ReturnedResourceArray& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override {}
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;

  // Resets test data.
  void Reset();

  // Runs all queued messages.
  void Flush();

 private:
  mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient> binding_;
  cc::mojom::MojoCompositorFrameSinkPtr sink_;
  bool did_receive_ack_ = false;
  cc::ReturnedResourceArray last_reclaimed_resources_;

  DISALLOW_COPY_AND_ASSIGN(FakeRendererCompositorFrameSink);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_RENDERER_COMPOSITOR_FRAME_SINK_H_

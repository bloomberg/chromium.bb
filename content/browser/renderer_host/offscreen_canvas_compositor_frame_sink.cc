// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// static
void OffscreenCanvasCompositorFrameSink::Create(
    const cc::FrameSinkId& frame_sink_id,
    cc::SurfaceManager* surface_manager,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::MojoCompositorFrameSinkRequest request) {
  std::unique_ptr<OffscreenCanvasCompositorFrameSink> impl =
      base::MakeUnique<OffscreenCanvasCompositorFrameSink>(
          frame_sink_id, surface_manager, std::move(client));
  OffscreenCanvasCompositorFrameSink* compositor_frame_sink = impl.get();
  compositor_frame_sink->binding_ =
      mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

OffscreenCanvasCompositorFrameSink::OffscreenCanvasCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::SurfaceManager* surface_manager,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : support_(this, surface_manager, frame_sink_id, nullptr, nullptr),
      client_(std::move(client)) {}

OffscreenCanvasCompositorFrameSink::~OffscreenCanvasCompositorFrameSink() {}

void OffscreenCanvasCompositorFrameSink::SetNeedsBeginFrame(
    bool needs_begin_frame) {
  support_.SetNeedsBeginFrame(needs_begin_frame);
}

void OffscreenCanvasCompositorFrameSink::SubmitCompositorFrame(
    const cc::LocalFrameId& local_frame_id,
    cc::CompositorFrame frame) {
  support_.SubmitCompositorFrame(local_frame_id, std::move(frame));
}

void OffscreenCanvasCompositorFrameSink::AddSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  // TODO(fsamuel, staraz): Implement this.
  NOTIMPLEMENTED();
}

void OffscreenCanvasCompositorFrameSink::RemoveSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  // TODO(fsamuel, staraz): Implement this.
  NOTIMPLEMENTED();
}

void OffscreenCanvasCompositorFrameSink::EvictFrame() {
  support_.EvictFrame();
}

void OffscreenCanvasCompositorFrameSink::Require(
    const cc::LocalFrameId& local_frame_id,
    const cc::SurfaceSequence& sequence) {
  support_.Require(local_frame_id, sequence);
}

void OffscreenCanvasCompositorFrameSink::Satisfy(
    const cc::SurfaceSequence& sequence) {
  support_.Satisfy(sequence);
}

void OffscreenCanvasCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (client_)
    client_->DidReceiveCompositorFrameAck();
}

void OffscreenCanvasCompositorFrameSink::OnBeginFrame(
    const cc::BeginFrameArgs& args) {
  if (client_)
    client_->OnBeginFrame(args);
}

void OffscreenCanvasCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  if (client_)
    client_->ReclaimResources(resources);
}

void OffscreenCanvasCompositorFrameSink::WillDrawSurface() {
  if (client_)
    client_->WillDrawSurface();
}

}  // namespace content

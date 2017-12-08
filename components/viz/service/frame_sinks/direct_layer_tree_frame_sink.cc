// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h"

#include "base/bind.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"

namespace viz {

DirectLayerTreeFrameSink::DirectLayerTreeFrameSink(
    const FrameSinkId& frame_sink_id,
    CompositorFrameSinkSupportManager* support_manager,
    FrameSinkManagerImpl* frame_sink_manager,
    Display* display,
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    SharedBitmapManager* shared_bitmap_manager)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider),
                         gpu_memory_buffer_manager,
                         shared_bitmap_manager),
      frame_sink_id_(frame_sink_id),
      support_manager_(support_manager),
      frame_sink_manager_(frame_sink_manager),
      display_(display) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capabilities_.must_always_swap = true;
  // Display and DirectLayerTreeFrameSink share a GL context, so sync
  // points aren't needed when passing resources between them.
  capabilities_.delegated_sync_points_required = false;
}

DirectLayerTreeFrameSink::DirectLayerTreeFrameSink(
    const FrameSinkId& frame_sink_id,
    CompositorFrameSinkSupportManager* support_manager,
    FrameSinkManagerImpl* frame_sink_manager,
    Display* display,
    scoped_refptr<VulkanContextProvider> vulkan_context_provider)
    : LayerTreeFrameSink(std::move(vulkan_context_provider)),
      frame_sink_id_(frame_sink_id),
      support_manager_(support_manager),
      frame_sink_manager_(frame_sink_manager),
      display_(display) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  capabilities_.must_always_swap = true;
}

DirectLayerTreeFrameSink::~DirectLayerTreeFrameSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool DirectLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!cc::LayerTreeFrameSink::BindToClient(client))
    return false;

  constexpr bool is_root = true;
  support_ = support_manager_->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root,
      capabilities_.delegated_sync_points_required);
  begin_frame_source_ = base::MakeUnique<ExternalBeginFrameSource>(this);
  client_->SetBeginFrameSource(begin_frame_source_.get());

  // Avoid initializing GL context here, as this should be sharing the
  // Display's context.
  display_->Initialize(this, frame_sink_manager_->surface_manager());
  return true;
}

void DirectLayerTreeFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();

  // Unregister the SurfaceFactoryClient here instead of the dtor so that only
  // one client is alive for this namespace at any given time.
  support_.reset();

  cc::LayerTreeFrameSink::DetachFromClient();
}

void DirectLayerTreeFrameSink::SubmitCompositorFrame(CompositorFrame frame) {
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  if (!local_surface_id_.is_valid() ||
      frame.size_in_pixels() != last_swap_frame_size_ ||
      frame.device_scale_factor() != device_scale_factor_) {
    local_surface_id_ = parent_local_surface_id_allocator_.GenerateId();
    last_swap_frame_size_ = frame.size_in_pixels();
    device_scale_factor_ = frame.device_scale_factor();
    display_->SetLocalSurfaceId(local_surface_id_, device_scale_factor_);
  }

  bool result =
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  DCHECK(result);
}

void DirectLayerTreeFrameSink::DidNotProduceFrame(const BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  support_->DidNotProduceFrame(ack);
}

void DirectLayerTreeFrameSink::DisplayOutputSurfaceLost() {
  is_lost_ = true;
  client_->DidLoseLayerTreeFrameSink();
}

void DirectLayerTreeFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const RenderPassList& render_passes) {
  // This notification is not relevant to our client outside of tests.
}

void DirectLayerTreeFrameSink::DisplayDidDrawAndSwap() {
  // This notification is not relevant to our client outside of tests. We
  // unblock the client from DidDrawCallback() when the surface is going to
  // be drawn.
}

void DirectLayerTreeFrameSink::DisplayDidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  // TODO(ccameron): Continue plumbing |ca_layer_params| through to
  // ui::AcceleratedWidgetMac.
}

void DirectLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void DirectLayerTreeFrameSink::DidPresentCompositorFrame(
    uint32_t presentation_token,
    base::TimeTicks time,
    base::TimeDelta refresh,
    uint32_t flags) {
  client_->DidPresentCompositorFrame(presentation_token, time, refresh, flags);
}

void DirectLayerTreeFrameSink::DidDiscardCompositorFrame(
    uint32_t presentation_token) {
  client_->DidDiscardCompositorFrame(presentation_token);
}

void DirectLayerTreeFrameSink::OnBeginFrame(const BeginFrameArgs& args) {
  begin_frame_source_->OnBeginFrame(args);
}

void DirectLayerTreeFrameSink::ReclaimResources(
    const std::vector<ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
}

void DirectLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void DirectLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void DirectLayerTreeFrameSink::OnContextLost() {
  // The display will be listening for OnContextLost(). Do nothing here.
}

}  // namespace viz

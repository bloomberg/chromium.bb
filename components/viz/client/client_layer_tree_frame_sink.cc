// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_layer_tree_frame_sink.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"

namespace viz {

ClientLayerTreeFrameSink::ClientLayerTreeFrameSink(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    SharedBitmapManager* shared_bitmap_manager,
    std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source,
    mojom::CompositorFrameSinkPtrInfo compositor_frame_sink_info,
    mojom::CompositorFrameSinkClientRequest client_request,
    std::unique_ptr<LocalSurfaceIdProvider> local_surface_id_provider,
    bool enable_surface_synchronization)
    : cc::LayerTreeFrameSink(std::move(context_provider),
                             std::move(worker_context_provider),
                             gpu_memory_buffer_manager,
                             shared_bitmap_manager),
      local_surface_id_provider_(std::move(local_surface_id_provider)),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)),
      compositor_frame_sink_info_(std::move(compositor_frame_sink_info)),
      client_request_(std::move(client_request)),
      client_binding_(this),
      enable_surface_synchronization_(enable_surface_synchronization),
      weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

ClientLayerTreeFrameSink::ClientLayerTreeFrameSink(
    scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
    std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source,
    mojom::CompositorFrameSinkPtrInfo compositor_frame_sink_info,
    mojom::CompositorFrameSinkClientRequest client_request,
    std::unique_ptr<LocalSurfaceIdProvider> local_surface_id_provider,
    bool enable_surface_synchronization)
    : cc::LayerTreeFrameSink(std::move(vulkan_context_provider)),
      local_surface_id_provider_(std::move(local_surface_id_provider)),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)),
      compositor_frame_sink_info_(std::move(compositor_frame_sink_info)),
      client_request_(std::move(client_request)),
      client_binding_(this),
      enable_surface_synchronization_(enable_surface_synchronization),
      weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

ClientLayerTreeFrameSink::~ClientLayerTreeFrameSink() {}

base::WeakPtr<ClientLayerTreeFrameSink> ClientLayerTreeFrameSink::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return weak_factory_.GetWeakPtr();
}

bool ClientLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!cc::LayerTreeFrameSink::BindToClient(client))
    return false;

  compositor_frame_sink_.Bind(std::move(compositor_frame_sink_info_));
  compositor_frame_sink_.set_connection_error_with_reason_handler(
      base::Bind(ClientLayerTreeFrameSink::OnMojoConnectionError));
  client_binding_.Bind(std::move(client_request_));

  if (synthetic_begin_frame_source_) {
    client->SetBeginFrameSource(synthetic_begin_frame_source_.get());
  } else {
    begin_frame_source_ = base::MakeUnique<ExternalBeginFrameSource>(this);
    begin_frame_source_->OnSetBeginFrameSourcePaused(begin_frames_paused_);
    client->SetBeginFrameSource(begin_frame_source_.get());
  }

  return true;
}

void ClientLayerTreeFrameSink::DetachFromClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  synthetic_begin_frame_source_.reset();
  client_binding_.Close();
  compositor_frame_sink_.reset();
  cc::LayerTreeFrameSink::DetachFromClient();
}

void ClientLayerTreeFrameSink::SetLocalSurfaceId(
    const LocalSurfaceId& local_surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(local_surface_id.is_valid());
  DCHECK(enable_surface_synchronization_);
  local_surface_id_ = local_surface_id;
}

void ClientLayerTreeFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  if (!enable_surface_synchronization_) {
    local_surface_id_ =
        local_surface_id_provider_->GetLocalSurfaceIdForFrame(frame);
  }

  compositor_frame_sink_->SubmitCompositorFrame(local_surface_id_,
                                                std::move(frame));
}

void ClientLayerTreeFrameSink::DidNotProduceFrame(const BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  compositor_frame_sink_->DidNotProduceFrame(ack);
}

void ClientLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<cc::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void ClientLayerTreeFrameSink::OnBeginFrame(
    const BeginFrameArgs& begin_frame_args) {
  if (begin_frame_source_)
    begin_frame_source_->OnBeginFrame(begin_frame_args);
}

void ClientLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  begin_frames_paused_ = paused;
  if (begin_frame_source_)
    begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void ClientLayerTreeFrameSink::ReclaimResources(
    const std::vector<cc::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
}

void ClientLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  compositor_frame_sink_->SetNeedsBeginFrame(needs_begin_frames);
}

// static
void ClientLayerTreeFrameSink::OnMojoConnectionError(
    uint32_t custom_reason,
    const std::string& description) {
  if (custom_reason)
    DLOG(FATAL) << description;
}

}  // namespace viz

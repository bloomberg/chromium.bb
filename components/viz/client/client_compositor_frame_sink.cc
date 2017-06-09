// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_compositor_frame_sink.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "components/viz/client/local_surface_id_provider.h"

namespace viz {

ClientCompositorFrameSink::ClientCompositorFrameSink(
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    cc::SharedBitmapManager* shared_bitmap_manager,
    std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source,
    cc::mojom::MojoCompositorFrameSinkPtrInfo compositor_frame_sink_info,
    cc::mojom::MojoCompositorFrameSinkClientRequest client_request,
    std::unique_ptr<LocalSurfaceIdProvider> local_surface_id_provider,
    bool enable_surface_synchronization)
    : cc::CompositorFrameSink(std::move(context_provider),
                              std::move(worker_context_provider),
                              gpu_memory_buffer_manager,
                              shared_bitmap_manager),
      local_surface_id_provider_(std::move(local_surface_id_provider)),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)),
      compositor_frame_sink_info_(std::move(compositor_frame_sink_info)),
      client_request_(std::move(client_request)),
      client_binding_(this),
      enable_surface_synchronization_(enable_surface_synchronization) {
  DETACH_FROM_THREAD(thread_checker_);
}

ClientCompositorFrameSink::ClientCompositorFrameSink(
    scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
    std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source,
    cc::mojom::MojoCompositorFrameSinkPtrInfo compositor_frame_sink_info,
    cc::mojom::MojoCompositorFrameSinkClientRequest client_request,
    std::unique_ptr<LocalSurfaceIdProvider> local_surface_id_provider,
    bool enable_surface_synchronization)
    : cc::CompositorFrameSink(std::move(vulkan_context_provider)),
      local_surface_id_provider_(std::move(local_surface_id_provider)),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)),
      compositor_frame_sink_info_(std::move(compositor_frame_sink_info)),
      client_request_(std::move(client_request)),
      client_binding_(this),
      enable_surface_synchronization_(enable_surface_synchronization) {
  DETACH_FROM_THREAD(thread_checker_);
}

ClientCompositorFrameSink::~ClientCompositorFrameSink() {}

bool ClientCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!cc::CompositorFrameSink::BindToClient(client))
    return false;

  compositor_frame_sink_.Bind(std::move(compositor_frame_sink_info_));
  compositor_frame_sink_.set_connection_error_with_reason_handler(
      base::Bind(ClientCompositorFrameSink::OnMojoConnectionError));
  client_binding_.Bind(std::move(client_request_));

  if (synthetic_begin_frame_source_) {
    client->SetBeginFrameSource(synthetic_begin_frame_source_.get());
  } else {
    begin_frame_source_ = base::MakeUnique<cc::ExternalBeginFrameSource>(this);
    client->SetBeginFrameSource(begin_frame_source_.get());
  }

  return true;
}

void ClientCompositorFrameSink::DetachFromClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  synthetic_begin_frame_source_.reset();
  client_binding_.Close();
  compositor_frame_sink_.reset();
  cc::CompositorFrameSink::DetachFromClient();
}

void ClientCompositorFrameSink::SetLocalSurfaceId(
    const cc::LocalSurfaceId& local_surface_id) {
  DCHECK(local_surface_id.is_valid());
  DCHECK(enable_surface_synchronization_);
  local_surface_id_ = local_surface_id;
}

void ClientCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(cc::BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  if (!enable_surface_synchronization_) {
    local_surface_id_ =
        local_surface_id_provider_->GetLocalSurfaceIdForFrame(frame);
  }

  compositor_frame_sink_->SubmitCompositorFrame(local_surface_id_,
                                                std::move(frame));
}

void ClientCompositorFrameSink::DidNotProduceFrame(
    const cc::BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(cc::BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  compositor_frame_sink_->DidNotProduceFrame(ack);
}

void ClientCompositorFrameSink::DidReceiveCompositorFrameAck(
    const cc::ReturnedResourceArray& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void ClientCompositorFrameSink::OnBeginFrame(
    const cc::BeginFrameArgs& begin_frame_args) {
  if (begin_frame_source_)
    begin_frame_source_->OnBeginFrame(begin_frame_args);
}

void ClientCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
}

void ClientCompositorFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  compositor_frame_sink_->SetNeedsBeginFrame(needs_begin_frames);
}

// static
void ClientCompositorFrameSink::OnMojoConnectionError(
    uint32_t custom_reason,
    const std::string& description) {
  if (custom_reason)
    DLOG(FATAL) << description;
}

}  // namespace viz

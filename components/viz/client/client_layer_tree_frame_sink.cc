// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_layer_tree_frame_sink.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/client/hit_test_data_provider.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"

namespace viz {

ClientLayerTreeFrameSink::InitParams::InitParams() = default;

ClientLayerTreeFrameSink::InitParams::~InitParams() = default;

ClientLayerTreeFrameSink::UnboundMessagePipes::UnboundMessagePipes() = default;

ClientLayerTreeFrameSink::UnboundMessagePipes::~UnboundMessagePipes() = default;

bool ClientLayerTreeFrameSink::UnboundMessagePipes::HasUnbound() const {
  return client_request.is_pending() &&
         (compositor_frame_sink_info.is_valid() ^
          compositor_frame_sink_associated_info.is_valid());
}

ClientLayerTreeFrameSink::UnboundMessagePipes::UnboundMessagePipes(
    UnboundMessagePipes&& other) = default;

ClientLayerTreeFrameSink::ClientLayerTreeFrameSink(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    InitParams* params)
    : cc::LayerTreeFrameSink(std::move(context_provider),
                             std::move(worker_context_provider),
                             params->gpu_memory_buffer_manager,
                             params->shared_bitmap_manager),
      hit_test_data_provider_(std::move(params->hit_test_data_provider)),
      local_surface_id_provider_(std::move(params->local_surface_id_provider)),
      synthetic_begin_frame_source_(
          std::move(params->synthetic_begin_frame_source)),
      pipes_(std::move(params->pipes)),
      client_binding_(this),
      enable_surface_synchronization_(params->enable_surface_synchronization),
      weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

ClientLayerTreeFrameSink::ClientLayerTreeFrameSink(
    scoped_refptr<VulkanContextProvider> vulkan_context_provider,
    InitParams* params)
    : cc::LayerTreeFrameSink(std::move(vulkan_context_provider)),
      hit_test_data_provider_(std::move(params->hit_test_data_provider)),
      local_surface_id_provider_(std::move(params->local_surface_id_provider)),
      synthetic_begin_frame_source_(
          std::move(params->synthetic_begin_frame_source)),
      pipes_(std::move(params->pipes)),
      client_binding_(this),
      enable_surface_synchronization_(params->enable_surface_synchronization),
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

  DCHECK(pipes_.HasUnbound());
  if (pipes_.compositor_frame_sink_info.is_valid()) {
    compositor_frame_sink_.Bind(std::move(pipes_.compositor_frame_sink_info));
    compositor_frame_sink_.set_connection_error_with_reason_handler(
        base::Bind(&ClientLayerTreeFrameSink::OnMojoConnectionError,
                   weak_factory_.GetWeakPtr()));
    compositor_frame_sink_ptr_ = compositor_frame_sink_.get();
  } else if (pipes_.compositor_frame_sink_associated_info.is_valid()) {
    compositor_frame_sink_associated_.Bind(
        std::move(pipes_.compositor_frame_sink_associated_info));
    compositor_frame_sink_associated_.set_connection_error_with_reason_handler(
        base::Bind(&ClientLayerTreeFrameSink::OnMojoConnectionError,
                   weak_factory_.GetWeakPtr()));
    compositor_frame_sink_ptr_ = compositor_frame_sink_associated_.get();
  }
  client_binding_.Bind(std::move(pipes_.client_request));

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
  compositor_frame_sink_associated_.reset();
  compositor_frame_sink_ptr_ = nullptr;
  cc::LayerTreeFrameSink::DetachFromClient();
}

void ClientLayerTreeFrameSink::SetLocalSurfaceId(
    const LocalSurfaceId& local_surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(local_surface_id.is_valid());
  DCHECK(enable_surface_synchronization_);
  local_surface_id_ = local_surface_id;
}

void ClientLayerTreeFrameSink::SubmitCompositorFrame(CompositorFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  if (!enable_surface_synchronization_) {
    local_surface_id_ =
        local_surface_id_provider_->GetLocalSurfaceIdForFrame(frame);
  }

  TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                          "SubmitCompositorFrame",
                          local_surface_id_.parent_id());
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                                     &tracing_enabled);

  mojom::HitTestRegionListPtr hit_test_region_list;
  if (hit_test_data_provider_)
    hit_test_region_list = hit_test_data_provider_->GetHitTestData();

  compositor_frame_sink_ptr_->SubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::move(hit_test_region_list),
      tracing_enabled ? base::TimeTicks::Now().since_origin().InMicroseconds()
                      : 0);
}

void ClientLayerTreeFrameSink::DidNotProduceFrame(const BeginFrameAck& ack) {
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(!ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  compositor_frame_sink_ptr_->DidNotProduceFrame(ack);
}

void ClientLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void ClientLayerTreeFrameSink::DidPresentCompositorFrame(
    uint32_t presentation_token,
    base::TimeTicks time,
    base::TimeDelta refresh,
    uint32_t flags) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->DidPresentCompositorFrame(presentation_token, time, refresh, flags);
}

void ClientLayerTreeFrameSink::DidDiscardCompositorFrame(
    uint32_t presentation_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->DidDiscardCompositorFrame(presentation_token);
}

void ClientLayerTreeFrameSink::OnBeginFrame(const BeginFrameArgs& args) {
  if (!needs_begin_frames_) {
    // We had a race with SetNeedsBeginFrame(false) and still need to let the
    // sink know that we didn't use this BeginFrame.
    DidNotProduceFrame(
        BeginFrameAck(args.source_id, args.sequence_number, false));
  }
  if (begin_frame_source_)
    begin_frame_source_->OnBeginFrame(args);
}

void ClientLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  begin_frames_paused_ = paused;
  if (begin_frame_source_)
    begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void ClientLayerTreeFrameSink::ReclaimResources(
    const std::vector<ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
}

void ClientLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK(compositor_frame_sink_ptr_);
  needs_begin_frames_ = needs_begin_frames;
  compositor_frame_sink_ptr_->SetNeedsBeginFrame(needs_begin_frames);
}

void ClientLayerTreeFrameSink::OnMojoConnectionError(
    uint32_t custom_reason,
    const std::string& description) {
  if (custom_reason)
    DLOG(FATAL) << description;
  if (client_)
    client_->DidLoseLayerTreeFrameSink();
}

}  // namespace viz

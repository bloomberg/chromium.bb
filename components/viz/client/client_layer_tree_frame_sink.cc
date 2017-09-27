// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_layer_tree_frame_sink.h"

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
ClientLayerTreeFrameSink::InitParams::InitParams() {}

ClientLayerTreeFrameSink::InitParams::~InitParams() {}

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
      compositor_frame_sink_info_(
          std::move(params->compositor_frame_sink_info)),
      client_request_(std::move(params->client_request)),
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
      compositor_frame_sink_info_(
          std::move(params->compositor_frame_sink_info)),
      client_request_(std::move(params->client_request)),
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

void ClientLayerTreeFrameSink::SubmitCompositorFrame(CompositorFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  if (!enable_surface_synchronization_) {
    local_surface_id_ =
        local_surface_id_provider_->GetLocalSurfaceIdForFrame(frame);
  }

  TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                          "SubmitCompositorFrame",
                          local_surface_id_.local_id());
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                                     &tracing_enabled);

  mojom::HitTestRegionListPtr hit_test_region_list;
  if (hit_test_data_provider_)
    hit_test_region_list = hit_test_data_provider_->GetHitTestData();

  compositor_frame_sink_->SubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::move(hit_test_region_list),
      tracing_enabled ? base::TimeTicks::Now().since_origin().InMicroseconds()
                      : 0);
}

void ClientLayerTreeFrameSink::DidNotProduceFrame(const BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  compositor_frame_sink_->DidNotProduceFrame(ack);
}

void ClientLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
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
  needs_begin_frames_ = needs_begin_frames;
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

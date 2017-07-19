// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_layer_tree_frame_sink.h"

#include <stdint.h>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "cc/output/output_surface.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

namespace viz {

static constexpr FrameSinkId kLayerTreeFrameSinkId(1, 1);

TestLayerTreeFrameSink::TestLayerTreeFrameSink(
    scoped_refptr<ContextProvider> compositor_context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const cc::RendererSettings& renderer_settings,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool synchronous_composite,
    bool disable_display_vsync,
    double refresh_rate)
    : LayerTreeFrameSink(std::move(compositor_context_provider),
                         std::move(worker_context_provider),
                         gpu_memory_buffer_manager,
                         shared_bitmap_manager),
      synchronous_composite_(synchronous_composite),
      disable_display_vsync_(disable_display_vsync),
      renderer_settings_(renderer_settings),
      refresh_rate_(refresh_rate),
      task_runner_(std::move(task_runner)),
      frame_sink_id_(kLayerTreeFrameSinkId),
      local_surface_id_allocator_(new LocalSurfaceIdAllocator),
      external_begin_frame_source_(this),
      weak_ptr_factory_(this) {
  // Always use sync tokens so that code paths in resource provider that deal
  // with sync tokens are tested.
  capabilities_.delegated_sync_points_required = true;
}

TestLayerTreeFrameSink::~TestLayerTreeFrameSink() {
  DCHECK(copy_requests_.empty());
}

void TestLayerTreeFrameSink::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  copy_requests_.push_back(std::move(request));
}

bool TestLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* client) {
  if (!LayerTreeFrameSink::BindToClient(client))
    return false;

  frame_sink_manager_ = base::MakeUnique<FrameSinkManagerImpl>();

  std::unique_ptr<cc::OutputSurface> display_output_surface =
      test_client_->CreateDisplayOutputSurface(context_provider());
  bool display_context_shared_with_compositor =
      display_output_surface->context_provider() == context_provider();

  std::unique_ptr<DisplayScheduler> scheduler;
  if (!synchronous_composite_) {
    if (disable_display_vsync_) {
      begin_frame_source_ = base::MakeUnique<cc::BackToBackBeginFrameSource>(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get()));
    } else {
      begin_frame_source_ = base::MakeUnique<cc::DelayBasedBeginFrameSource>(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get()));
      begin_frame_source_->SetAuthoritativeVSyncInterval(
          base::TimeDelta::FromMilliseconds(1000.f / refresh_rate_));
    }
    scheduler = base::MakeUnique<DisplayScheduler>(
        begin_frame_source_.get(), task_runner_.get(),
        display_output_surface->capabilities().max_frames_pending);
  }

  display_ = base::MakeUnique<Display>(
      shared_bitmap_manager(), gpu_memory_buffer_manager(), renderer_settings_,
      frame_sink_id_, std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get()));

  // We want the Display's OutputSurface to hear about lost context, and when
  // this shares a context with it we should not be listening for lost context
  // callbacks on the context here.
  if (display_context_shared_with_compositor && context_provider())
    context_provider()->SetLostContextCallback(base::Closure());

  constexpr bool is_root = false;
  constexpr bool handles_frame_sink_id_invalidation = true;
  constexpr bool needs_sync_points = true;
  support_ = CompositorFrameSinkSupport::Create(
      this, frame_sink_manager_.get(), frame_sink_id_, is_root,
      handles_frame_sink_id_invalidation, needs_sync_points);
  client_->SetBeginFrameSource(&external_begin_frame_source_);
  if (begin_frame_source_) {
    frame_sink_manager_->RegisterBeginFrameSource(begin_frame_source_.get(),
                                                  frame_sink_id_);
  }
  display_->Initialize(this, frame_sink_manager_->surface_manager());
  display_->renderer_for_testing()->SetEnlargePassTextureAmountForTesting(
      enlarge_pass_texture_amount_);
  display_->SetVisible(true);
  return true;
}

void TestLayerTreeFrameSink::DetachFromClient() {
  if (begin_frame_source_)
    frame_sink_manager_->UnregisterBeginFrameSource(begin_frame_source_.get());
  client_->SetBeginFrameSource(nullptr);
  support_ = nullptr;
  display_ = nullptr;
  begin_frame_source_ = nullptr;
  local_surface_id_allocator_ = nullptr;
  frame_sink_manager_ = nullptr;
  test_client_ = nullptr;
  LayerTreeFrameSink::DetachFromClient();
}

void TestLayerTreeFrameSink::SetLocalSurfaceId(
    const LocalSurfaceId& local_surface_id) {
  test_client_->DisplayReceivedLocalSurfaceId(local_surface_id);
}

void TestLayerTreeFrameSink::SubmitCompositorFrame(cc::CompositorFrame frame) {
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(cc::BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);
  test_client_->DisplayReceivedCompositorFrame(frame);

  gfx::Size frame_size = frame.render_pass_list.back()->output_rect.size();
  float device_scale_factor = frame.metadata.device_scale_factor;
  if (!local_surface_id_.is_valid() || frame_size != display_size_ ||
      device_scale_factor != device_scale_factor_) {
    local_surface_id_ = local_surface_id_allocator_->GenerateId();
    display_->SetLocalSurfaceId(local_surface_id_, device_scale_factor);
    display_->Resize(frame_size);
    display_size_ = frame_size;
    device_scale_factor_ = device_scale_factor;
  }

  bool result =
      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  DCHECK(result);

  for (auto& copy_request : copy_requests_)
    support_->RequestCopyOfSurface(std::move(copy_request));
  copy_requests_.clear();

  if (!display_->has_scheduler()) {
    display_->DrawAndSwap();
    // Post this to get a new stack frame so that we exit this function before
    // calling the client to tell it that it is done.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestLayerTreeFrameSink::SendCompositorFrameAckToClient,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void TestLayerTreeFrameSink::DidNotProduceFrame(const cc::BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(cc::BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  support_->DidNotProduceFrame(ack);
}

void TestLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<cc::ReturnedResource>& resources) {
  ReclaimResources(resources);
  // In synchronous mode, we manually send acks and this method should not be
  // used.
  if (!display_->has_scheduler())
    return;
  client_->DidReceiveCompositorFrameAck();
}

void TestLayerTreeFrameSink::OnBeginFrame(const cc::BeginFrameArgs& args) {
  external_begin_frame_source_.OnBeginFrame(args);
}

void TestLayerTreeFrameSink::ReclaimResources(
    const std::vector<cc::ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
}

void TestLayerTreeFrameSink::WillDrawSurface(
    const LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void TestLayerTreeFrameSink::DisplayOutputSurfaceLost() {
  client_->DidLoseLayerTreeFrameSink();
}

void TestLayerTreeFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const cc::RenderPassList& render_passes) {
  test_client_->DisplayWillDrawAndSwap(will_draw_and_swap, render_passes);
}

void TestLayerTreeFrameSink::DisplayDidDrawAndSwap() {
  test_client_->DisplayDidDrawAndSwap();
}

void TestLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void TestLayerTreeFrameSink::SendCompositorFrameAckToClient() {
  client_->DidReceiveCompositorFrameAck();
}

}  // namespace viz

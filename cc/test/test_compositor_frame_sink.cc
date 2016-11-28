// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_compositor_frame_sink.h"

#include <stdint.h>
#include <utility>

#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/output_surface.h"
#include "cc/output/texture_mailbox_deleter.h"

namespace cc {

static constexpr FrameSinkId kCompositorFrameSinkId(1, 1);

TestCompositorFrameSink::TestCompositorFrameSink(
    scoped_refptr<ContextProvider> compositor_context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const RendererSettings& renderer_settings,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool synchronous_composite,
    bool force_disable_reclaim_resources)
    : CompositorFrameSink(std::move(compositor_context_provider),
                          std::move(worker_context_provider),
                          gpu_memory_buffer_manager,
                          shared_bitmap_manager),
      synchronous_composite_(synchronous_composite),
      renderer_settings_(renderer_settings),
      task_runner_(std::move(task_runner)),
      frame_sink_id_(kCompositorFrameSinkId),
      surface_manager_(new SurfaceManager),
      surface_id_allocator_(new SurfaceIdAllocator()),
      surface_factory_(
          new SurfaceFactory(frame_sink_id_, surface_manager_.get(), this)),
      weak_ptr_factory_(this) {
  // Since this CompositorFrameSink and the Display are tightly coupled and in
  // the same process/thread, the LayerTreeHostImpl can reclaim resources from
  // the Display. But we allow tests to disable this to mimic an out-of-process
  // Display.
  capabilities_.can_force_reclaim_resources = !force_disable_reclaim_resources;
  // Always use sync tokens so that code paths in resource provider that deal
  // with sync tokens are tested.
  capabilities_.delegated_sync_points_required = true;
}

TestCompositorFrameSink::~TestCompositorFrameSink() {
  DCHECK(copy_requests_.empty());
}

void TestCompositorFrameSink::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> request) {
  copy_requests_.push_back(std::move(request));
}

bool TestCompositorFrameSink::BindToClient(CompositorFrameSinkClient* client) {
  if (!CompositorFrameSink::BindToClient(client))
    return false;

  std::unique_ptr<OutputSurface> display_output_surface =
      test_client_->CreateDisplayOutputSurface(context_provider());
  bool display_context_shared_with_compositor =
      display_output_surface->context_provider() == context_provider();

  std::unique_ptr<SyntheticBeginFrameSource> begin_frame_source;
  std::unique_ptr<DisplayScheduler> scheduler;
  if (!synchronous_composite_) {
    if (renderer_settings_.disable_display_vsync) {
      begin_frame_source.reset(new BackToBackBeginFrameSource(
          base::MakeUnique<DelayBasedTimeSource>(task_runner_.get())));
    } else {
      begin_frame_source.reset(new DelayBasedBeginFrameSource(
          base::MakeUnique<DelayBasedTimeSource>(task_runner_.get())));
      begin_frame_source->SetAuthoritativeVSyncInterval(
          base::TimeDelta::FromMilliseconds(1000.f /
                                            renderer_settings_.refresh_rate));
    }
    scheduler.reset(new DisplayScheduler(
        begin_frame_source.get(), task_runner_.get(),
        display_output_surface->capabilities().max_frames_pending));
  }

  display_.reset(new Display(
      shared_bitmap_manager(), gpu_memory_buffer_manager(), renderer_settings_,
      frame_sink_id_, std::move(begin_frame_source),
      std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<TextureMailboxDeleter>(task_runner_.get())));

  // We want the Display's OutputSurface to hear about lost context, and when
  // this shares a context with it we should not be listening for lost context
  // callbacks on the context here.
  if (display_context_shared_with_compositor && context_provider())
    context_provider()->SetLostContextCallback(base::Closure());

  surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  surface_manager_->RegisterSurfaceFactoryClient(frame_sink_id_, this);
  display_->Initialize(this, surface_manager_.get());
  display_->renderer_for_testing()->SetEnlargePassTextureAmountForTesting(
      enlarge_pass_texture_amount_);
  display_->SetVisible(true);
  bound_ = true;
  return true;
}

void TestCompositorFrameSink::DetachFromClient() {
  // Some tests make BindToClient fail on purpose. ^__^
  if (bound_) {
    surface_factory_->EvictSurface();
    surface_manager_->UnregisterSurfaceFactoryClient(frame_sink_id_);
    surface_manager_->InvalidateFrameSinkId(frame_sink_id_);
    display_ = nullptr;
    bound_ = false;
  }
  surface_factory_ = nullptr;
  surface_id_allocator_ = nullptr;
  surface_manager_ = nullptr;
  test_client_ = nullptr;
  CompositorFrameSink::DetachFromClient();
}

void TestCompositorFrameSink::SubmitCompositorFrame(CompositorFrame frame) {
  test_client_->DisplayReceivedCompositorFrame(frame);

  if (!delegated_local_frame_id_.is_valid()) {
    delegated_local_frame_id_ = surface_id_allocator_->GenerateId();
  }
  display_->SetLocalFrameId(delegated_local_frame_id_,
                            frame.metadata.device_scale_factor);

  gfx::Size frame_size = frame.render_pass_list.back()->output_rect.size();
  display_->Resize(frame_size);

  bool synchronous = !display_->has_scheduler();

  SurfaceFactory::DrawCallback draw_callback;
  if (!synchronous) {
    // For async draws, we use a callback tell when it is done, but for sync
    // draws we don't need one. Unretained is safe here because the callback
    // will be run when |surface_factory_| is destroyed which is owned by this
    // class.
    draw_callback = base::Bind(&TestCompositorFrameSink::DidDrawCallback,
                               base::Unretained(this));
  }

  surface_factory_->SubmitCompositorFrame(delegated_local_frame_id_,
                                          std::move(frame), draw_callback);

  for (std::unique_ptr<CopyOutputRequest>& copy_request : copy_requests_) {
    surface_factory_->RequestCopyOfSurface(std::move(copy_request));
  }
  copy_requests_.clear();

  if (synchronous) {
    display_->DrawAndSwap();
    // Post this to get a new stack frame so that we exit this function before
    // calling the client to tell it that it is done.
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&TestCompositorFrameSink::DidDrawCallback,
                                      weak_ptr_factory_.GetWeakPtr()));
  }
}

void TestCompositorFrameSink::DidDrawCallback() {
  // This is to unthrottle the next frame, not actually a notice that drawing is
  // done.
  client_->DidReceiveCompositorFrameAck();
}

void TestCompositorFrameSink::ForceReclaimResources() {
  if (capabilities_.can_force_reclaim_resources &&
      delegated_local_frame_id_.is_valid()) {
    surface_factory_->ClearSurface();
  }
}

void TestCompositorFrameSink::ReturnResources(
    const ReturnedResourceArray& resources) {
  client_->ReclaimResources(resources);
}

void TestCompositorFrameSink::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  client_->SetBeginFrameSource(begin_frame_source);
}

void TestCompositorFrameSink::DisplayOutputSurfaceLost() {
  client_->DidLoseCompositorFrameSink();
}

void TestCompositorFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const RenderPassList& render_passes) {
  test_client_->DisplayWillDrawAndSwap(will_draw_and_swap, render_passes);
}

void TestCompositorFrameSink::DisplayDidDrawAndSwap() {
  test_client_->DisplayDidDrawAndSwap();
}

}  // namespace cc

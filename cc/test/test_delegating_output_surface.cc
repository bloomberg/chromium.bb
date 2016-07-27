// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_delegating_output_surface.h"

#include <stdint.h>
#include <utility>

#include "cc/output/begin_frame_args.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/test/begin_frame_args_test.h"

static constexpr uint32_t kCompositorClientId = 1;

namespace cc {

TestDelegatingOutputSurface::TestDelegatingOutputSurface(
    scoped_refptr<ContextProvider> compositor_context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    std::unique_ptr<OutputSurface> display_output_surface,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const RendererSettings& renderer_settings,
    base::SingleThreadTaskRunner* task_runner,
    bool synchronous_composite)
    : OutputSurface(std::move(compositor_context_provider),
                    std::move(worker_context_provider),
                    nullptr),
      surface_manager_(new SurfaceManager),
      surface_id_allocator_(new SurfaceIdAllocator(kCompositorClientId)),
      surface_factory_(new SurfaceFactory(surface_manager_.get(), this)),
      weak_ptrs_(this) {
  std::unique_ptr<SyntheticBeginFrameSource> begin_frame_source;
  std::unique_ptr<DisplayScheduler> scheduler;
  if (!synchronous_composite) {
    begin_frame_source.reset(new DelayBasedBeginFrameSource(
        base::MakeUnique<DelayBasedTimeSource>(task_runner)));
    scheduler.reset(new DisplayScheduler(
        begin_frame_source.get(), task_runner,
        display_output_surface->capabilities().max_frames_pending));
  }
  const bool context_shared_with_compositor =
      display_output_surface->context_provider() == context_provider();
  display_.reset(
      new Display(shared_bitmap_manager, gpu_memory_buffer_manager,
                  renderer_settings, std::move(begin_frame_source),
                  std::move(display_output_surface), std::move(scheduler),
                  base::MakeUnique<TextureMailboxDeleter>(task_runner)));

  capabilities_.delegated_rendering = true;
  // Since this OutputSurface and the Display are tightly coupled and in the
  // same process/thread, the LayerTreeHostImpl can reclaim resources from
  // the Display.
  capabilities_.can_force_reclaim_resources = true;
  capabilities_.delegated_sync_points_required =
      !context_shared_with_compositor;
}

TestDelegatingOutputSurface::~TestDelegatingOutputSurface() {
  DCHECK(copy_requests_.empty());
}

void TestDelegatingOutputSurface::RequestCopyOfOutput(
    std::unique_ptr<CopyOutputRequest> request) {
  copy_requests_.push_back(std::move(request));
}

bool TestDelegatingOutputSurface::BindToClient(OutputSurfaceClient* client) {
  if (!OutputSurface::BindToClient(client))
    return false;

  // We want the Display's output surface to hear about lost context, and since
  // this shares a context with it (when delegated_sync_points_required is
  // false), we should not be listening for lost context callbacks on the
  // context here.
  if (!capabilities_.delegated_sync_points_required && context_provider())
    context_provider()->SetLostContextCallback(base::Closure());

  surface_manager_->RegisterSurfaceClientId(surface_id_allocator_->client_id());
  surface_manager_->RegisterSurfaceFactoryClient(
      surface_id_allocator_->client_id(), this);
  display_->Initialize(this, surface_manager_.get(),
                       surface_id_allocator_->client_id());
  bound_ = true;
  return true;
}

void TestDelegatingOutputSurface::DetachFromClient() {
  // Some tests make BindToClient fail on purpose. ^__^
  if (bound_) {
    if (!delegated_surface_id_.is_null())
      surface_factory_->Destroy(delegated_surface_id_);
    surface_manager_->UnregisterSurfaceFactoryClient(
        surface_id_allocator_->client_id());
    surface_manager_->InvalidateSurfaceClientId(
        surface_id_allocator_->client_id());
    bound_ = false;
  }
  display_ = nullptr;
  surface_factory_ = nullptr;
  surface_id_allocator_ = nullptr;
  surface_manager_ = nullptr;
  weak_ptrs_.InvalidateWeakPtrs();
  OutputSurface::DetachFromClient();
}

void TestDelegatingOutputSurface::SwapBuffers(CompositorFrame frame) {
  if (delegated_surface_id_.is_null()) {
    delegated_surface_id_ = surface_id_allocator_->GenerateId();
    surface_factory_->Create(delegated_surface_id_);
  }
  display_->SetSurfaceId(delegated_surface_id_,
                         frame.metadata.device_scale_factor);

  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list.back()->output_rect.size();
  display_->Resize(frame_size);

  surface_factory_->SubmitCompositorFrame(
      delegated_surface_id_, std::move(frame),
      base::Bind(&TestDelegatingOutputSurface::DrawCallback,
                 weak_ptrs_.GetWeakPtr()));

  for (std::unique_ptr<CopyOutputRequest>& copy_request : copy_requests_)
    surface_factory_->RequestCopyOfSurface(delegated_surface_id_,
                                           std::move(copy_request));
  copy_requests_.clear();

  if (!display_->has_scheduler())
    display_->DrawAndSwap();
}

void TestDelegatingOutputSurface::DrawCallback() {
  client_->DidSwapBuffersComplete();
}

void TestDelegatingOutputSurface::ForceReclaimResources() {
  if (capabilities_.can_force_reclaim_resources &&
      !delegated_surface_id_.is_null()) {
    surface_factory_->SubmitCompositorFrame(delegated_surface_id_,
                                            CompositorFrame(),
                                            SurfaceFactory::DrawCallback());
  }
}

void TestDelegatingOutputSurface::BindFramebuffer() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
}

uint32_t TestDelegatingOutputSurface::GetFramebufferCopyTextureFormat() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
  return 0;
}

void TestDelegatingOutputSurface::ReturnResources(
    const ReturnedResourceArray& resources) {
  client_->ReclaimResources(resources);
}

void TestDelegatingOutputSurface::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  client_->SetBeginFrameSource(begin_frame_source);
}

void TestDelegatingOutputSurface::DisplayOutputSurfaceLost() {
  DidLoseOutputSurface();
}

void TestDelegatingOutputSurface::DisplaySetMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  SetMemoryPolicy(policy);
}

}  // namespace cc

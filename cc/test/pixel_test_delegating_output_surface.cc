// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_delegating_output_surface.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/pixel_test_software_output_device.h"
#include "cc/test/test_in_process_context_provider.h"

static constexpr uint32_t kCompositorSurfaceNamespace = 1;

namespace cc {

PixelTestDelegatingOutputSurface::PixelTestDelegatingOutputSurface(
    scoped_refptr<ContextProvider> compositor_context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    scoped_refptr<ContextProvider> display_context_provider,
    const RendererSettings& renderer_settings,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gfx::Size& surface_expansion_size,
    bool allow_force_reclaim_resources,
    bool synchronous_composite)
    : OutputSurface(std::move(compositor_context_provider),
                    std::move(worker_context_provider),
                    nullptr),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      surface_expansion_size_(surface_expansion_size),
      allow_force_reclaim_resources_(allow_force_reclaim_resources),
      synchronous_composite_(synchronous_composite),
      renderer_settings_(renderer_settings),
      display_context_provider_(std::move(display_context_provider)),
      surface_manager_(new SurfaceManager),
      surface_id_allocator_(
          new SurfaceIdAllocator(kCompositorSurfaceNamespace)),
      surface_factory_(new SurfaceFactory(surface_manager_.get(), this)),
      weak_ptrs_(this) {
  capabilities_.delegated_rendering = true;
  capabilities_.can_force_reclaim_resources = allow_force_reclaim_resources_;

  surface_id_allocator_->RegisterSurfaceIdNamespace(surface_manager_.get());
}

PixelTestDelegatingOutputSurface::~PixelTestDelegatingOutputSurface() {}

bool PixelTestDelegatingOutputSurface::BindToClient(
    OutputSurfaceClient* client) {
  if (!OutputSurface::BindToClient(client))
    return false;

  surface_manager_->RegisterSurfaceFactoryClient(
      surface_id_allocator_->id_namespace(), this);

  // The PixelTestOutputSurface is owned by the Display.
  std::unique_ptr<PixelTestOutputSurface> output_surface;

  if (!context_provider()) {
    std::unique_ptr<PixelTestSoftwareOutputDevice> software_output_device(
        new PixelTestSoftwareOutputDevice);
    software_output_device->set_surface_expansion_size(surface_expansion_size_);
    output_surface = base::MakeUnique<PixelTestOutputSurface>(
        std::move(software_output_device));
  } else {
    bool flipped_output_surface = false;
    output_surface = base::MakeUnique<PixelTestOutputSurface>(
        std::move(display_context_provider_), nullptr, flipped_output_surface);
  }
  output_surface->set_surface_expansion_size(surface_expansion_size_);

  auto* task_runner = base::ThreadTaskRunnerHandle::Get().get();
  CHECK(task_runner);

  std::unique_ptr<SyntheticBeginFrameSource> begin_frame_source;
  std::unique_ptr<DisplayScheduler> scheduler;
  if (!synchronous_composite_) {
    begin_frame_source.reset(new DelayBasedBeginFrameSource(
        base::MakeUnique<DelayBasedTimeSource>(task_runner)));
    scheduler.reset(new DisplayScheduler(
        begin_frame_source.get(), task_runner,
        output_surface->capabilities().max_frames_pending));
  }

  display_.reset(new Display(
      surface_manager_.get(), shared_bitmap_manager_,
      gpu_memory_buffer_manager_, renderer_settings_,
      surface_id_allocator_->id_namespace(), std::move(begin_frame_source),
      std::move(output_surface), std::move(scheduler),
      base::MakeUnique<TextureMailboxDeleter>(task_runner)));
  display_->SetEnlargePassTextureAmountForTesting(enlarge_pass_texture_amount_);

  display_->Initialize(&display_client_);
  return true;
}

void PixelTestDelegatingOutputSurface::DetachFromClient() {
  if (!delegated_surface_id_.is_null())
    surface_factory_->Destroy(delegated_surface_id_);
  surface_manager_->UnregisterSurfaceFactoryClient(
      surface_id_allocator_->id_namespace());

  display_ = nullptr;
  surface_factory_ = nullptr;
  surface_id_allocator_ = nullptr;
  surface_manager_ = nullptr;
  weak_ptrs_.InvalidateWeakPtrs();
  OutputSurface::DetachFromClient();
}

void PixelTestDelegatingOutputSurface::SwapBuffers(CompositorFrame frame) {
  client_->DidSwapBuffers();

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
      base::Bind(&PixelTestDelegatingOutputSurface::DrawCallback,
                 weak_ptrs_.GetWeakPtr()));

  if (synchronous_composite_)
    display_->DrawAndSwap();
}

void PixelTestDelegatingOutputSurface::SetEnlargePassTextureAmount(
    const gfx::Size& amount) {
  DCHECK(!HasClient());
  enlarge_pass_texture_amount_ = amount;
}

void PixelTestDelegatingOutputSurface::DrawCallback(SurfaceDrawStatus) {
  client_->DidSwapBuffersComplete();
}

void PixelTestDelegatingOutputSurface::ForceReclaimResources() {
  if (allow_force_reclaim_resources_ && !delegated_surface_id_.is_null()) {
    surface_factory_->SubmitCompositorFrame(delegated_surface_id_,
                                            CompositorFrame(),
                                            SurfaceFactory::DrawCallback());
  }
}

void PixelTestDelegatingOutputSurface::BindFramebuffer() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
}

uint32_t PixelTestDelegatingOutputSurface::GetFramebufferCopyTextureFormat() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
  return 0;
}

void PixelTestDelegatingOutputSurface::ReturnResources(
    const ReturnedResourceArray& resources) {
  CompositorFrameAck ack;
  ack.resources = resources;
  client_->ReclaimResources(&ack);
}

void PixelTestDelegatingOutputSurface::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  client_->SetBeginFrameSource(begin_frame_source);
}

}  // namespace cc

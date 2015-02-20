// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display.h"

#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/software_renderer.h"
#include "cc/resources/texture_mailbox_deleter.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/blocking_task_runner.h"

namespace cc {

Display::Display(DisplayClient* client,
                 SurfaceManager* manager,
                 SharedBitmapManager* bitmap_manager,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                 const RendererSettings& settings)
    : client_(client),
      manager_(manager),
      bitmap_manager_(bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      settings_(settings),
      device_scale_factor_(1.f),
      blocking_main_thread_task_runner_(
          BlockingTaskRunner::Create(base::MessageLoopProxy::current())),
      texture_mailbox_deleter_(
          new TextureMailboxDeleter(base::MessageLoopProxy::current())) {
  manager_->AddObserver(this);
}

Display::~Display() {
  manager_->RemoveObserver(this);
  if (aggregator_) {
    for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
      Surface* surface = manager_->GetSurfaceForId(id_entry.first);
      if (surface)
        surface->RunDrawCallbacks(SurfaceDrawStatus::DRAW_SKIPPED);
    }
  }
}

bool Display::Initialize(scoped_ptr<OutputSurface> output_surface) {
  output_surface_ = output_surface.Pass();
  return output_surface_->BindToClient(this);
}

void Display::SetSurfaceId(SurfaceId id, float device_scale_factor) {
  current_surface_id_ = id;
  device_scale_factor_ = device_scale_factor;
  client_->DisplayDamaged();
}

void Display::Resize(const gfx::Size& size) {
  if (size == current_surface_size_)
    return;
  // Need to ensure all pending swaps have executed before the window is
  // resized, or D3D11 will scale the swap output.
  if (renderer_ && settings_.finish_rendering_on_resize)
    renderer_->Finish();
  current_surface_size_ = size;
  client_->DisplayDamaged();
}

void Display::InitializeRenderer() {
  if (resource_provider_)
    return;

  scoped_ptr<ResourceProvider> resource_provider = ResourceProvider::Create(
      output_surface_.get(), bitmap_manager_, gpu_memory_buffer_manager_,
      blocking_main_thread_task_runner_.get(), settings_.highp_threshold_min,
      settings_.use_rgba_4444_textures,
      settings_.texture_id_allocation_chunk_size);
  if (!resource_provider)
    return;

  if (output_surface_->context_provider()) {
    scoped_ptr<GLRenderer> renderer = GLRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider.get(),
        texture_mailbox_deleter_.get(), settings_.highp_threshold_min);
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  } else {
    scoped_ptr<SoftwareRenderer> renderer = SoftwareRenderer::Create(
        this, &settings_, output_surface_.get(), resource_provider.get());
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  }

  resource_provider_ = resource_provider.Pass();
  aggregator_.reset(new SurfaceAggregator(manager_, resource_provider_.get()));
}

void Display::DidLoseOutputSurface() {
  client_->OutputSurfaceLost();
}

bool Display::Draw() {
  if (current_surface_id_.is_null())
    return false;

  InitializeRenderer();
  if (!output_surface_)
    return false;

  // TODO(skyostil): We should hold a BlockingTaskRunner::CapturePostTasks
  // while Aggregate is called to immediately run release callbacks afterward.
  scoped_ptr<CompositorFrame> frame =
      aggregator_->Aggregate(current_surface_id_);
  if (!frame)
    return false;

  TRACE_EVENT0("cc", "Display::Draw");
  benchmark_instrumentation::IssueDisplayRenderingStatsEvent();

  // Run callbacks early to allow pipelining.
  for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
    Surface* surface = manager_->GetSurfaceForId(id_entry.first);
    if (surface)
      surface->RunDrawCallbacks(SurfaceDrawStatus::DRAWN);
  }
  DelegatedFrameData* frame_data = frame->delegated_frame_data.get();

  frame->metadata.latency_info.insert(frame->metadata.latency_info.end(),
                                      stored_latency_info_.begin(),
                                      stored_latency_info_.end());
  stored_latency_info_.clear();
  bool have_copy_requests = false;
  for (const auto* pass : frame_data->render_pass_list) {
    have_copy_requests |= !pass->copy_requests.empty();
  }

  gfx::Size surface_size;
  bool have_damage = false;
  if (!frame_data->render_pass_list.empty()) {
    surface_size = frame_data->render_pass_list.back()->output_rect.size();
    have_damage =
        !frame_data->render_pass_list.back()->damage_rect.size().IsEmpty();
  }
  bool avoid_swap = surface_size != current_surface_size_;
  bool should_draw = !frame->metadata.latency_info.empty() ||
                     have_copy_requests || (have_damage && !avoid_swap);

  if (should_draw) {
    gfx::Rect device_viewport_rect = gfx::Rect(current_surface_size_);
    gfx::Rect device_clip_rect = device_viewport_rect;
    bool disable_picture_quad_image_filtering = false;

    renderer_->DecideRenderPassAllocationsForFrame(
        frame_data->render_pass_list);
    renderer_->DrawFrame(&frame_data->render_pass_list, device_scale_factor_,
                         device_viewport_rect, device_clip_rect,
                         disable_picture_quad_image_filtering);
  }

  if (should_draw && !avoid_swap) {
    renderer_->SwapBuffers(frame->metadata);
  } else {
    stored_latency_info_.insert(stored_latency_info_.end(),
                                frame->metadata.latency_info.begin(),
                                frame->metadata.latency_info.end());
    DidSwapBuffers();
    DidSwapBuffersComplete();
  }

  return true;
}

void Display::DidSwapBuffers() {
  client_->DidSwapBuffers();
}

void Display::DidSwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

void Display::CommitVSyncParameters(base::TimeTicks timebase,
                                    base::TimeDelta interval) {
  client_->CommitVSyncParameters(timebase, interval);
}

void Display::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  client_->SetMemoryPolicy(policy);
}

void Display::OnSurfaceDamaged(SurfaceId surface_id, bool* changed) {
  if (aggregator_ &&
      aggregator_->previous_contained_surfaces().count(surface_id)) {
    Surface* surface = manager_->GetSurfaceForId(surface_id);
    if (surface) {
      const CompositorFrame* current_frame = surface->GetEligibleFrame();
      if (!current_frame || !current_frame->delegated_frame_data ||
          !current_frame->delegated_frame_data->resource_list.size())
        aggregator_->ReleaseResources(surface_id);
    }
    client_->DisplayDamaged();
    *changed = true;
  } else if (surface_id == current_surface_id_) {
    client_->DisplayDamaged();
    *changed = true;
  }
}

SurfaceId Display::CurrentSurfaceId() {
  return current_surface_id_;
}

int Display::GetMaxFramesPending() {
  int max_frames_pending =
      output_surface_ ? output_surface_->capabilities().max_frames_pending : 0;
  if (max_frames_pending <= 0)
    max_frames_pending = OutputSurface::DEFAULT_MAX_FRAMES_PENDING;
  return max_frames_pending;
}

}  // namespace cc

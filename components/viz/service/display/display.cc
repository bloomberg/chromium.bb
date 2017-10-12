// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/display/texture_mailbox_deleter.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/vulkan/features.h"
#include "ui/gfx/buffer_types.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "cc/output/vulkan_renderer.h"
#endif

namespace viz {

Display::Display(SharedBitmapManager* bitmap_manager,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                 const RendererSettings& settings,
                 const FrameSinkId& frame_sink_id,
                 std::unique_ptr<OutputSurface> output_surface,
                 std::unique_ptr<DisplayScheduler> scheduler,
                 std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter)
    : bitmap_manager_(bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      settings_(settings),
      frame_sink_id_(frame_sink_id),
      output_surface_(std::move(output_surface)),
      scheduler_(std::move(scheduler)),
      texture_mailbox_deleter_(std::move(texture_mailbox_deleter)) {
  DCHECK(output_surface_);
  DCHECK(frame_sink_id_.is_valid());
  if (scheduler_)
    scheduler_->SetClient(this);
}

Display::~Display() {
  // Only do this if Initialize() happened.
  if (client_) {
    if (auto* context = output_surface_->context_provider())
      context->SetLostContextCallback(base::Closure());
    if (scheduler_)
      surface_manager_->RemoveObserver(scheduler_.get());
  }
  if (aggregator_) {
    for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
      Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
      if (surface)
        surface->RunDrawCallback();
    }
  }
}

void Display::Initialize(DisplayClient* client,
                         SurfaceManager* surface_manager) {
  DCHECK(client);
  DCHECK(surface_manager);
  client_ = client;
  surface_manager_ = surface_manager;
  if (scheduler_)
    surface_manager_->AddObserver(scheduler_.get());

  output_surface_->BindToClient(this);
  InitializeRenderer();

  if (auto* context = output_surface_->context_provider()) {
    // This depends on assumptions that Display::Initialize will happen
    // on the same callstack as the ContextProvider being created/initialized
    // or else it could miss a callback before setting this.
    context->SetLostContextCallback(base::Bind(
        &Display::DidLoseContextProvider,
        // Unretained is safe since the callback is unset in this class'
        // destructor and is never posted.
        base::Unretained(this)));
  }
}

void Display::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void Display::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Display::SetLocalSurfaceId(const LocalSurfaceId& id,
                                float device_scale_factor) {
  if (current_surface_id_.local_surface_id() == id &&
      device_scale_factor_ == device_scale_factor) {
    return;
  }

  TRACE_EVENT0("viz", "Display::SetSurfaceId");
  current_surface_id_ = SurfaceId(frame_sink_id_, id);
  device_scale_factor_ = device_scale_factor;

  UpdateRootSurfaceResourcesLocked();
  if (scheduler_)
    scheduler_->SetNewRootSurface(current_surface_id_);
}

void Display::SetVisible(bool visible) {
  TRACE_EVENT1("viz", "Display::SetVisible", "visible", visible);
  if (renderer_)
    renderer_->SetVisible(visible);
  if (scheduler_)
    scheduler_->SetVisible(visible);
  visible_ = visible;

  if (!visible) {
    // Damage tracker needs a full reset as renderer resources are dropped when
    // not visible.
    if (aggregator_ && current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::Resize(const gfx::Size& size) {
  if (size == current_surface_size_)
    return;

  TRACE_EVENT0("viz", "Display::Resize");

  // Need to ensure all pending swaps have executed before the window is
  // resized, or D3D11 will scale the swap output.
  if (settings_.finish_rendering_on_resize) {
    if (!swapped_since_resize_ && scheduler_)
      scheduler_->ForceImmediateSwapIfPossible();
    if (swapped_since_resize_ && output_surface_ &&
        output_surface_->context_provider())
      output_surface_->context_provider()->ContextGL()->ShallowFinishCHROMIUM();
  }
  swapped_since_resize_ = false;
  current_surface_size_ = size;
  if (scheduler_)
    scheduler_->DisplayResized();
}

void Display::SetColorSpace(const gfx::ColorSpace& blending_color_space,
                            const gfx::ColorSpace& device_color_space) {
  blending_color_space_ = blending_color_space;
  device_color_space_ = device_color_space;
  if (aggregator_) {
    aggregator_->SetOutputColorSpace(blending_color_space, device_color_space_);
  }
}

void Display::SetOutputIsSecure(bool secure) {
  if (secure == output_is_secure_)
    return;
  output_is_secure_ = secure;

  if (aggregator_) {
    aggregator_->set_output_is_secure(secure);
    // Force a redraw.
    if (current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::InitializeRenderer() {
  resource_provider_ = base::MakeUnique<cc::DisplayResourceProvider>(
      output_surface_->context_provider(), bitmap_manager_,
      gpu_memory_buffer_manager_, settings_.resource_settings);

  if (output_surface_->context_provider()) {
    DCHECK(texture_mailbox_deleter_);
    if (!settings_.use_skia_renderer) {
      renderer_ = base::MakeUnique<GLRenderer>(
          &settings_, output_surface_.get(), resource_provider_.get(),
          texture_mailbox_deleter_.get());
    } else {
      renderer_ = base::MakeUnique<SkiaRenderer>(
          &settings_, output_surface_.get(), resource_provider_.get());
    }
  } else if (output_surface_->vulkan_context_provider()) {
#if defined(ENABLE_VULKAN)
    DCHECK(texture_mailbox_deleter_);
    renderer_ = base::MakeUnique<VulkanRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get());
#else
    NOTREACHED();
#endif
  } else {
    auto renderer = base::MakeUnique<SoftwareRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get());
    software_renderer_ = renderer.get();
    renderer_ = std::move(renderer);
  }

  renderer_->Initialize();
  renderer_->SetVisible(visible_);

  // TODO(jbauman): Outputting an incomplete quad list doesn't work when using
  // overlays.
  bool output_partial_list = renderer_->use_partial_swap() &&
                             !output_surface_->GetOverlayCandidateValidator();
  aggregator_.reset(new SurfaceAggregator(
      surface_manager_, resource_provider_.get(), output_partial_list));
  aggregator_->set_output_is_secure(output_is_secure_);
  aggregator_->SetOutputColorSpace(blending_color_space_, device_color_space_);
}

void Display::UpdateRootSurfaceResourcesLocked() {
  Surface* surface = surface_manager_->GetSurfaceForId(current_surface_id_);
  bool root_surface_resources_locked = !surface || !surface->HasActiveFrame();
  if (scheduler_)
    scheduler_->SetRootSurfaceResourcesLocked(root_surface_resources_locked);
}

void Display::DidLoseContextProvider() {
  if (scheduler_)
    scheduler_->OutputSurfaceLost();
  // WARNING: The client may delete the Display in this method call. Do not
  // make any additional references to members after this call.
  client_->DisplayOutputSurfaceLost();
}

bool Display::DrawAndSwap() {
  TRACE_EVENT0("viz", "Display::DrawAndSwap");

  if (!current_surface_id_.is_valid()) {
    TRACE_EVENT_INSTANT0("viz", "No root surface.", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (!output_surface_) {
    TRACE_EVENT_INSTANT0("viz", "No output surface", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  base::ElapsedTimer aggregate_timer;
  CompositorFrame frame = aggregator_->Aggregate(current_surface_id_);
  UMA_HISTOGRAM_COUNTS_1M("Compositing.SurfaceAggregator.AggregateUs",
                          aggregate_timer.Elapsed().InMicroseconds());

  if (frame.render_pass_list.empty()) {
    TRACE_EVENT_INSTANT0("viz", "Empty aggregated frame.",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // Run callbacks early to allow pipelining.
  for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
    Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
    if (surface)
      surface->RunDrawCallback();
  }

  frame.metadata.latency_info.insert(frame.metadata.latency_info.end(),
                                     stored_latency_info_.begin(),
                                     stored_latency_info_.end());
  stored_latency_info_.clear();
  bool have_copy_requests = false;
  for (const auto& pass : frame.render_pass_list) {
    have_copy_requests |= !pass->copy_requests.empty();
  }

  gfx::Size surface_size;
  bool have_damage = false;
  auto& last_render_pass = *frame.render_pass_list.back();
  if (last_render_pass.output_rect.size() != current_surface_size_ &&
      last_render_pass.damage_rect == last_render_pass.output_rect &&
      !current_surface_size_.IsEmpty()) {
    // Resize the output rect to the current surface size so that we won't
    // skip the draw and so that the GL swap won't stretch the output.
    last_render_pass.output_rect.set_size(current_surface_size_);
    last_render_pass.damage_rect = last_render_pass.output_rect;
  }
  surface_size = last_render_pass.output_rect.size();
  have_damage = !last_render_pass.damage_rect.size().IsEmpty();

  bool size_matches = surface_size == current_surface_size_;
  if (!size_matches)
    TRACE_EVENT_INSTANT0("viz", "Size mismatch.", TRACE_EVENT_SCOPE_THREAD);

  bool should_draw = have_copy_requests || (have_damage && size_matches);

  // If the surface is suspended then the resources to be used by the draw are
  // likely destroyed.
  if (output_surface_->SurfaceIsSuspendForRecycle()) {
    TRACE_EVENT_INSTANT0("viz", "Surface is suspended for recycle.",
                         TRACE_EVENT_SCOPE_THREAD);
    should_draw = false;
  }

  client_->DisplayWillDrawAndSwap(should_draw, frame.render_pass_list);

  if (should_draw) {
    bool disable_image_filtering =
        frame.metadata.is_resourceless_software_draw_with_scroll_or_animation;
    if (software_renderer_) {
      software_renderer_->SetDisablePictureQuadImageFiltering(
          disable_image_filtering);
    } else {
      // This should only be set for software draws in synchronous compositor.
      DCHECK(!disable_image_filtering);
    }

    base::ElapsedTimer draw_timer;
    renderer_->DecideRenderPassAllocationsForFrame(frame.render_pass_list);
    renderer_->DrawFrame(&frame.render_pass_list, device_scale_factor_,
                         current_surface_size_);
    if (software_renderer_) {
      UMA_HISTOGRAM_COUNTS_1M("Compositing.DirectRenderer.Software.DrawFrameUs",
                              draw_timer.Elapsed().InMicroseconds());
    } else {
      UMA_HISTOGRAM_COUNTS_1M("Compositing.DirectRenderer.GL.DrawFrameUs",
                              draw_timer.Elapsed().InMicroseconds());
    }
  } else {
    TRACE_EVENT_INSTANT0("viz", "Draw skipped.", TRACE_EVENT_SCOPE_THREAD);
  }

  bool should_swap = should_draw && size_matches;
  if (should_swap) {
    swapped_since_resize_ = true;
    for (auto& latency : frame.metadata.latency_info) {
      TRACE_EVENT_WITH_FLOW1(
          "input,benchmark", "LatencyInfo.Flow",
          TRACE_ID_DONT_MANGLE(latency.trace_id()),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
          "Display::DrawAndSwap");
    }
    cc::benchmark_instrumentation::IssueDisplayRenderingStatsEvent();
    renderer_->SwapBuffers(std::move(frame.metadata.latency_info));
    if (scheduler_)
      scheduler_->DidSwapBuffers();
  } else {
    if (have_damage && !size_matches)
      aggregator_->SetFullDamageForSurface(current_surface_id_);
    TRACE_EVENT_INSTANT0("viz", "Swap skipped.", TRACE_EVENT_SCOPE_THREAD);

    // Do not store more that the allowed size.
    if (ui::LatencyInfo::Verify(frame.metadata.latency_info,
                                "Display::DrawAndSwap")) {
      stored_latency_info_.insert(stored_latency_info_.end(),
                                  frame.metadata.latency_info.begin(),
                                  frame.metadata.latency_info.end());
    }

    if (scheduler_) {
      scheduler_->DidSwapBuffers();
      scheduler_->DidReceiveSwapBuffersAck();
    }
  }

  client_->DisplayDidDrawAndSwap();
  return true;
}

void Display::DidReceiveSwapBuffersAck() {
  if (scheduler_)
    scheduler_->DidReceiveSwapBuffersAck();
  if (renderer_)
    renderer_->SwapBuffersComplete();
}

void Display::DidReceiveTextureInUseResponses(
    const gpu::TextureInUseResponses& responses) {
  if (renderer_)
    renderer_->DidReceiveTextureInUseResponses(responses);
}

void Display::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  aggregator_->SetFullDamageForSurface(current_surface_id_);
  if (scheduler_) {
    BeginFrameAck ack;
    ack.has_damage = true;
    scheduler_->ProcessSurfaceDamage(current_surface_id_, ack, true);
  }
}

bool Display::SurfaceDamaged(const SurfaceId& surface_id,
                             const BeginFrameAck& ack) {
  bool display_damaged = false;
  if (ack.has_damage) {
    if (aggregator_ &&
        aggregator_->previous_contained_surfaces().count(surface_id)) {
      Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
      if (surface) {
        DCHECK(surface->HasActiveFrame());
        if (surface->GetActiveFrame().resource_list.empty())
          aggregator_->ReleaseResources(surface_id);
      }
      display_damaged = true;
      if (surface_id == current_surface_id_)
        UpdateRootSurfaceResourcesLocked();
    } else if (surface_id == current_surface_id_) {
      display_damaged = true;
      UpdateRootSurfaceResourcesLocked();
    }
  }

  return display_damaged;
}

void Display::SurfaceDiscarded(const SurfaceId& surface_id) {
  if (aggregator_)
    aggregator_->ReleaseResources(surface_id);
}

bool Display::SurfaceHasUndrawnFrame(const SurfaceId& surface_id) const {
  if (!surface_manager_)
    return false;

  Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
  if (!surface)
    return false;

  return surface->HasUndrawnActiveFrame();
}

void Display::DidFinishFrame(const BeginFrameAck& ack) {
  for (auto& observer : observers_)
    observer.OnDisplayDidFinishFrame(ack);
}

const SurfaceId& Display::CurrentSurfaceId() {
  return current_surface_id_;
}

void Display::ForceImmediateDrawAndSwapIfPossible() {
  if (scheduler_)
    scheduler_->ForceImmediateSwapIfPossible();
}

void Display::SetNeedsOneBeginFrame() {
  if (scheduler_)
    scheduler_->SetNeedsOneBeginFrame();
}

}  // namespace viz

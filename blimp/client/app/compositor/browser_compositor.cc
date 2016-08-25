// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/compositor/browser_compositor.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/feature/compositor/blimp_context_provider.h"
#include "blimp/client/feature/compositor/blimp_gpu_memory_buffer_manager.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface_display_output_surface.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "gpu/command_buffer/client/context_support.h"

namespace blimp {
namespace client {

namespace {

class SimpleTaskGraphRunner : public cc::SingleThreadTaskGraphRunner {
 public:
  SimpleTaskGraphRunner() {
    Start("BlimpBrowserCompositorWorker",
          base::SimpleThread::Options(base::ThreadPriority::BACKGROUND));
  }

  ~SimpleTaskGraphRunner() override { Shutdown(); }
};

class DisplayOutputSurface : public cc::OutputSurface {
 public:
  explicit DisplayOutputSurface(
      scoped_refptr<cc::ContextProvider> context_provider)
      : cc::OutputSurface(std::move(context_provider), nullptr, nullptr) {}

  ~DisplayOutputSurface() override = default;

  // cc::OutputSurface implementation
  void SwapBuffers(cc::CompositorFrame frame) override {
    // See cc::OutputSurface::SwapBuffers() comment for details.
    context_provider_->ContextSupport()->Swap();
    cc::OutputSurface::PostSwapBuffersComplete();
  }

  uint32_t GetFramebufferCopyTextureFormat() override {
    auto* gl = static_cast<BlimpContextProvider*>(context_provider());
    return gl->GetCopyTextureInternalFormat();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayOutputSurface);
};

base::LazyInstance<SimpleTaskGraphRunner> g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<cc::SurfaceManager> g_surface_manager =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<BlimpGpuMemoryBufferManager> g_gpu_memory_buffer_manager =
    LAZY_INSTANCE_INITIALIZER;

uint32_t g_surface_id = 0;

}  // namespace

// static
cc::SurfaceManager* BrowserCompositor::GetSurfaceManager() {
  return g_surface_manager.Pointer();
}

// static
BlimpGpuMemoryBufferManager* BrowserCompositor::GetGpuMemoryBufferManager() {
  return g_gpu_memory_buffer_manager.Pointer();
}

// static
uint32_t BrowserCompositor::AllocateSurfaceClientId() {
  return ++g_surface_id;
}

BrowserCompositor::BrowserCompositor()
    : surface_id_allocator_(base::MakeUnique<cc::SurfaceIdAllocator>(
          BrowserCompositor::AllocateSurfaceClientId())),
      widget_(gfx::kNullAcceleratedWidget),
      output_surface_request_pending_(false),
      root_layer_(cc::Layer::Create()) {
  BrowserCompositor::GetSurfaceManager()->RegisterSurfaceClientId(
      surface_id_allocator_->client_id());

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.gpu_memory_buffer_manager =
      BrowserCompositor::GetGpuMemoryBufferManager();
  params.task_graph_runner = g_task_graph_runner.Pointer();
  cc::LayerTreeSettings settings;
  params.settings = &settings;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.animation_host = cc::AnimationHost::CreateMainInstance();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);

  root_layer_->SetBackgroundColor(SK_ColorWHITE);
  host_->GetLayerTree()->SetRootLayer(root_layer_);
  host_->set_surface_client_id(surface_id_allocator_->client_id());
}

BrowserCompositor::~BrowserCompositor() {
  BrowserCompositor::GetSurfaceManager()->InvalidateSurfaceClientId(
      surface_id_allocator_->client_id());
}

void BrowserCompositor::SetContentLayer(
    scoped_refptr<cc::Layer> content_layer) {
  root_layer_->RemoveAllChildren();
  root_layer_->AddChild(content_layer);
}

void BrowserCompositor::SetSize(const gfx::Size& size_in_px) {
  viewport_size_in_px_ = size_in_px;

  // Update the host.
  host_->GetLayerTree()->SetViewportSize(viewport_size_in_px_);
  root_layer_->SetBounds(viewport_size_in_px_);

  // Update the display.
  if (display_) {
    display_->Resize(viewport_size_in_px_);
  }
}

void BrowserCompositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  // Kill all references to the old widget.
  if (widget_ != gfx::kNullAcceleratedWidget) {
    // We are always visible if we have a widget.
    DCHECK(host_->visible());
    host_->SetVisible(false);
    if (!host_->output_surface_lost()) {
      host_->ReleaseOutputSurface();
    }
    display_.reset();
  }

  widget_ = gfx::kNullAcceleratedWidget;

  if (widget != gfx::kNullAcceleratedWidget) {
    widget_ = widget;
    host_->SetVisible(true);
    if (output_surface_request_pending_)
      HandlePendingOutputSurfaceRequest();
  }
}

void BrowserCompositor::RequestNewOutputSurface() {
  DCHECK(!output_surface_request_pending_)
      << "We already have a pending request?";
  output_surface_request_pending_ = true;
  HandlePendingOutputSurfaceRequest();
}

void BrowserCompositor::DidInitializeOutputSurface() {
  output_surface_request_pending_ = false;
}

void BrowserCompositor::DidFailToInitializeOutputSurface() {
  NOTREACHED() << "Can't fail to initialize the OutputSurface here";
}

void BrowserCompositor::DidCompleteSwapBuffers() {
  if (!did_complete_swap_buffers_.is_null()) {
    did_complete_swap_buffers_.Run();
  }
}

void BrowserCompositor::HandlePendingOutputSurfaceRequest() {
  DCHECK(output_surface_request_pending_);

  // Can't handle the request right now since we don't have a widget.
  if (!host_->visible())
    return;

  DCHECK_NE(gfx::kNullAcceleratedWidget, widget_);

  scoped_refptr<cc::ContextProvider> context_provider =
      BlimpContextProvider::Create(
          widget_, BrowserCompositor::GetGpuMemoryBufferManager());

  std::unique_ptr<cc::OutputSurface> display_output_surface =
      base::MakeUnique<DisplayOutputSurface>(context_provider);

  auto* task_runner = base::ThreadTaskRunnerHandle::Get().get();
  std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner)));
  std::unique_ptr<cc::DisplayScheduler> scheduler(new cc::DisplayScheduler(
      begin_frame_source.get(), task_runner,
      display_output_surface->capabilities().max_frames_pending));

  display_ = base::MakeUnique<cc::Display>(
      nullptr /*shared_bitmap_manager*/,
      BrowserCompositor::GetGpuMemoryBufferManager(),
      host_->settings().renderer_settings, std::move(begin_frame_source),
      std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner));
  display_->SetVisible(true);
  display_->Resize(viewport_size_in_px_);

  // The Browser compositor and display share the same context provider.
  std::unique_ptr<cc::OutputSurface> delegated_output_surface =
      base::MakeUnique<cc::SurfaceDisplayOutputSurface>(
          BrowserCompositor::GetSurfaceManager(), surface_id_allocator_.get(),
          display_.get(), context_provider, nullptr);

  host_->SetOutputSurface(std::move(delegated_output_surface));
}

}  // namespace client
}  // namespace blimp

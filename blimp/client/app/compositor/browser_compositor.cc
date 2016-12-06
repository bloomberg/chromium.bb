// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/compositor/browser_compositor.h"

#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/client/support/compositor/blimp_context_provider.h"
#include "cc/output/context_provider.h"

#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

namespace blimp {
namespace client {

BrowserCompositor::BrowserCompositor(
    CompositorDependencies* compositor_dependencies)
    : BlimpEmbedderCompositor(compositor_dependencies) {}

BrowserCompositor::~BrowserCompositor() {
  SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

void BrowserCompositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  scoped_refptr<cc::ContextProvider> provider;

  if (surface_handle_ != gpu::kNullSurfaceHandle) {
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
    gpu::GpuSurfaceTracker::Get()->RemoveSurface(surface_handle_);
#endif
    surface_handle_ = gpu::kNullSurfaceHandle;
  }

  if (widget != gfx::kNullAcceleratedWidget) {
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
    surface_handle_ =
        gpu::GpuSurfaceTracker::Get()->AddSurfaceForNativeWidget(widget);
#else
    surface_handle_ = widget;
#endif
    provider = BlimpContextProvider::Create(
        surface_handle_,
        compositor_dependencies()->GetGpuMemoryBufferManager());
  }

  SetContextProvider(std::move(provider));
}

void BrowserCompositor::DidReceiveCompositorFrameAck() {
  if (!did_complete_swap_buffers_.is_null()) {
    did_complete_swap_buffers_.Run();
  }
}

}  // namespace client
}  // namespace blimp

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/onscreen_display_client.h"

#include "base/trace_event/trace_event.h"
#include "cc/output/output_surface.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_display_output_surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

OnscreenDisplayClient::OnscreenDisplayClient(
    std::unique_ptr<OutputSurface> output_surface,
    SurfaceManager* manager,
    SharedBitmapManager* bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const RendererSettings& settings,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    uint32_t compositor_surface_namespace)
    : output_surface_(std::move(output_surface)),
      task_runner_(task_runner),
      display_(new Display(this,
                           manager,
                           bitmap_manager,
                           gpu_memory_buffer_manager,
                           settings,
                           compositor_surface_namespace)),
      output_surface_lost_(false) {}

OnscreenDisplayClient::~OnscreenDisplayClient() {
}

bool OnscreenDisplayClient::Initialize() {
  DCHECK(output_surface_);
  return display_->Initialize(std::move(output_surface_), task_runner_.get());
}

void OnscreenDisplayClient::OutputSurfaceLost() {
  output_surface_lost_ = true;
  surface_display_output_surface_->DidLoseOutputSurface();
}

void OnscreenDisplayClient::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  surface_display_output_surface_->SetMemoryPolicy(policy);
}

}  // namespace cc

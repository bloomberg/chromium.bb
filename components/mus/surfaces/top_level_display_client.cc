// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/top_level_display_client.h"

#include "base/memory/ptr_util.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/display.h"

namespace mus {

TopLevelDisplayClient::TopLevelDisplayClient(
    std::unique_ptr<cc::OutputSurface> output_surface,
    cc::SurfaceManager* surface_manager,
    cc::SharedBitmapManager* bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const cc::RendererSettings& settings,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    uint32_t compositor_surface_namespace)
    : output_surface_(std::move(output_surface)),
      task_runner_(task_runner),
      display_(new cc::Display(this,
                               surface_manager,
                               bitmap_manager,
                               gpu_memory_buffer_manager,
                               settings,
                               compositor_surface_namespace)) {}

TopLevelDisplayClient::~TopLevelDisplayClient() {
}

bool TopLevelDisplayClient::Initialize() {
  DCHECK(output_surface_);
  return display_->Initialize(std::move(output_surface_), task_runner_.get());
}

void TopLevelDisplayClient::OutputSurfaceLost() {
  if (!display_)  // Shutdown case
    return;
  display_.reset();
}

void TopLevelDisplayClient::SetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {}

}  // namespace mus

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_mac.h"

#include "gpu/GLES2/gl2extchromium.h"

namespace viz {

GLOutputSurfaceMac::GLOutputSurfaceMac(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool allow_overlays)
    : GLOutputSurfaceBufferQueue(context_provider,
                                 surface_handle,
                                 gpu_memory_buffer_manager,
                                 gfx::BufferFormat::RGBA_8888),
      overlay_validator_(new OverlayCandidateValidatorMac(!allow_overlays)) {}

GLOutputSurfaceMac::~GLOutputSurfaceMac() {}

std::unique_ptr<OverlayCandidateValidator>
GLOutputSurfaceMac::TakeOverlayCandidateValidator() {
  return std::move(overlay_validator_);
}

}  // namespace viz

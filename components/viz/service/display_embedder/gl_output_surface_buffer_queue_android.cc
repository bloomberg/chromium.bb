// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue_android.h"

#include "third_party/khronos/GLES2/gl2.h"

namespace viz {

GLOutputSurfaceBufferQueueAndroid::GLOutputSurfaceBufferQueueAndroid(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle,
    SyntheticBeginFrameSource* synthetic_begin_frame_source,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferFormat buffer_format)
    : GLOutputSurfaceBufferQueue(context_provider,
                                 surface_handle,
                                 synthetic_begin_frame_source,
                                 gpu_memory_buffer_manager,
                                 GL_TEXTURE_2D,
                                 buffer_format) {}

GLOutputSurfaceBufferQueueAndroid::~GLOutputSurfaceBufferQueueAndroid() =
    default;

OverlayCandidateValidator*
GLOutputSurfaceBufferQueueAndroid::GetOverlayCandidateValidator() const {
  // TODO(khushalsagar): The API shouldn't be const.
  return &const_cast<GLOutputSurfaceBufferQueueAndroid*>(this)
              ->overlay_candidate_validator_;
}

}  // namespace viz

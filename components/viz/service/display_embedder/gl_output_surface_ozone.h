// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_OZONE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_OZONE_H_

#include <memory>
#include <vector>

#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue.h"

namespace viz {

class GLOutputSurfaceOzone : public GLOutputSurfaceBufferQueue {
 public:
  // |strategies| is a list of overlay strategies that will be attempted. If the
  // list is empty then there is no overlay support on this platform.
  GLOutputSurfaceOzone(
      scoped_refptr<VizProcessContextProvider> context_provider,
      gpu::SurfaceHandle surface_handle,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      std::vector<OverlayStrategy> strategies);
  ~GLOutputSurfaceOzone() override;

  // OutputSurface implementation.
  std::unique_ptr<OverlayCandidateValidator> TakeOverlayCandidateValidator()
      override;

 private:
  std::unique_ptr<OverlayCandidateValidator> overlay_candidate_validator_;

  DISALLOW_COPY_AND_ASSIGN(GLOutputSurfaceOzone);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_GL_OUTPUT_SURFACE_OZONE_H_

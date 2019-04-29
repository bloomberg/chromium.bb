// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/surface_handle.h"

class GrContext;

namespace gl {
class GLContext;
class GLSurface;
}  // namespace gl

namespace gpu {
namespace gles2 {
class FeatureInfo;
}  // namespace gles2
}  // namespace gpu

namespace viz {

class SkiaOutputDeviceGL final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceGL(
      gpu::SurfaceHandle surface_handle,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceGL() override;

  scoped_refptr<gl::GLSurface> gl_surface();
  void Initialize(GrContext* gr_context, gl::GLContext* gl_context);
  bool supports_alpha() {
    DCHECK(gr_context_);
    return supports_alpha_;
  }

  // SkiaOutputDevice implementation:
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  gfx::SwapResponse SwapBuffers(const GrBackendSemaphore& semaphore,
                                BufferPresentedCallback feedback) override;
  gfx::SwapResponse PostSubBuffer(const gfx::Rect& rect,
                                  const GrBackendSemaphore& semaphore,
                                  BufferPresentedCallback feedback) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;

 private:
  const gpu::SurfaceHandle surface_handle_;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  gpu::GpuPreferences gpu_preferences_;

  GrContext* gr_context_ = nullptr;
  scoped_refptr<gl::GLSurface> gl_surface_;

  bool supports_alpha_ = false;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceGL);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_GL_H_

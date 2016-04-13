// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_OZONE_H_
#define COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_OZONE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace ui {
class LatencyInfo;
}  // namespace ui

namespace cc {
class CompositorFrame;
}  // namespace cc

namespace mus {

class BufferQueue;
class SurfacesContextProvider;

// An OutputSurface implementation that directly draws and swap to a GL
// "surfaceless" surface (aka one backed by a buffer managed explicitly in
// mus/ozone. This class is adapted from
// GpuSurfacelessBrowserCompositorOutputSurface.
class DirectOutputSurfaceOzone : public cc::OutputSurface {
 public:
  DirectOutputSurfaceOzone(
      const scoped_refptr<SurfacesContextProvider>& context_provider,
      gfx::AcceleratedWidget widget,
      uint32_t target,
      uint32_t internalformat);

  ~DirectOutputSurfaceOzone() override;

  // TODO(rjkroege): Implement the equivalent of Reflector.

 private:
  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size, float scale_factor, bool alpha) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

  // Taken from BrowserCompositor specific API.
  void OnUpdateVSyncParametersFromGpu(base::TimeTicks timebase,
                                      base::TimeDelta interval);

  // Called when a swap completion is sent from the GPU process.
  void OnGpuSwapBuffersCompleted(gfx::SwapResult result);

  std::unique_ptr<BufferQueue> output_surface_;

  base::WeakPtrFactory<DirectOutputSurfaceOzone> weak_ptr_factory_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_DIRECT_OUTPUT_SURFACE_OZONE_H_

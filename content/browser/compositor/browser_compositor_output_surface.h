// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "cc/output/output_surface.h"
#include "content/common/content_export.h"
#include "ui/compositor/compositor_vsync_manager.h"

namespace cc {
class SoftwareOutputDevice;
}

namespace gfx {
enum class SwapResult;
}

namespace content {
class BrowserCompositorOverlayCandidateValidator;
class ContextProviderCommandBuffer;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;

class CONTENT_EXPORT BrowserCompositorOutputSurface
    : public cc::OutputSurface,
      public ui::CompositorVSyncManager::Observer {
 public:
  ~BrowserCompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override;

  // ui::CompositorOutputSurface::Observer implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  void OnUpdateVSyncParametersFromGpu(base::TimeTicks tiembase,
                                      base::TimeDelta interval);

  void SetReflector(ReflectorImpl* reflector);

  // Called when |reflector_| was updated.
  virtual void OnReflectorChanged();

  // Returns a callback that will be called when all mirroring
  // compositors have started composition.
  virtual base::Closure CreateCompositionStartedCallback();

  // Called when a swap completion is sent from the GPU process.
  virtual void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result) = 0;

#if defined(OS_MACOSX)
  virtual void SetSurfaceSuspendedForRecycle(bool suspended) = 0;
  virtual bool SurfaceShouldNotShowFramesAfterSuspendForRecycle() const = 0;
#endif

 protected:
  // Constructor used by the accelerated implementation.
  BrowserCompositorOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context,
      const scoped_refptr<cc::ContextProvider>& worker_context,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      scoped_ptr<BrowserCompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  // Constructor used by the software implementation.
  BrowserCompositorOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager);

  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;
  ReflectorImpl* reflector_;

  // True when BeginFrame scheduling is enabled.
  bool use_begin_frame_scheduling_;

 private:
  void Initialize();

  scoped_ptr<BrowserCompositorOverlayCandidateValidator>
      overlay_candidate_validator_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

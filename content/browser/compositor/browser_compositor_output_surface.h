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

namespace cc {
class SoftwareOutputDevice;
}

namespace display_compositor {
class CompositorOverlayCandidateValidator;
}

namespace gfx {
enum class SwapResult;
}

namespace content {
class ReflectorImpl;

class CONTENT_EXPORT BrowserCompositorOutputSurface
    : public cc::OutputSurface {
 public:
  using UpdateVSyncParametersCallback =
      base::Callback<void(base::TimeTicks timebase, base::TimeDelta interval)>;

  ~BrowserCompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;

  void SetReflector(ReflectorImpl* reflector);

  // Called when |reflector_| was updated.
  virtual void OnReflectorChanged();

#if defined(OS_MACOSX)
  virtual void SetSurfaceSuspendedForRecycle(bool suspended) = 0;
#endif

 protected:
  // Constructor used by the accelerated implementation.
  BrowserCompositorOutputSurface(
      scoped_refptr<cc::ContextProvider> context,
      const UpdateVSyncParametersCallback& update_vsync_parameters_callback,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator);

  // Constructor used by the software implementation.
  BrowserCompositorOutputSurface(
      std::unique_ptr<cc::SoftwareOutputDevice> software_device,
      const UpdateVSyncParametersCallback& update_vsync_parameters_callback);

  // Constructor used by the Vulkan implementation.
  BrowserCompositorOutputSurface(
      const scoped_refptr<cc::VulkanContextProvider>& vulkan_context_provider,
      const UpdateVSyncParametersCallback& update_vsync_parameters_callback);

  const UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  ReflectorImpl* reflector_;

 private:
  std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
      overlay_candidate_validator_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

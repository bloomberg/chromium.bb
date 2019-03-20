// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/ipc/common/surface_handle.h"

namespace gpu {
class VulkanSurface;
}

namespace viz {

class VulkanContextProvider;

class SkiaOutputDeviceVulkan : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceVulkan(VulkanContextProvider* context_provider,
                         gpu::SurfaceHandle surface_handle);
  ~SkiaOutputDeviceVulkan() override;

  sk_sp<SkSurface> DrawSurface() override;
  void Reshape(const gfx::Size& size) override;
  gfx::SwapResult SwapBuffers() override;

 private:
  void CreateVulkanSurface();
  void UpdateDrawSurface();

  VulkanContextProvider* const context_provider_;

  const gpu::SurfaceHandle surface_handle_;
  std::unique_ptr<gpu::VulkanSurface> vulkan_surface_;

  // SkSurfaces for swap chain images.
  std::vector<sk_sp<SkSurface>> sk_surfaces_;

  // SkSurface to be drawn to. Updated after Reshape and SwapBuffers.
  sk_sp<SkSurface> draw_surface_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceVulkan);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_

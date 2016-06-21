// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/raster/raster_buffer_provider.h"

namespace cc {
class ContextProvider;
class GpuRasterizer;
class ResourceProvider;

class CC_EXPORT GpuRasterBufferProvider : public RasterBufferProvider {
 public:
  GpuRasterBufferProvider(ContextProvider* compositor_context_provider,
                          ContextProvider* worker_context_provider,
                          ResourceProvider* resource_provider,
                          bool use_distance_field_text,
                          int gpu_rasterization_msaa_sample_count);
  ~GpuRasterBufferProvider() override;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override;
  void OrderingBarrier() override;
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override;
  void Shutdown() override;

 private:
  ContextProvider* const compositor_context_provider_;
  std::unique_ptr<GpuRasterizer> rasterizer_;

  DISALLOW_COPY_AND_ASSIGN(GpuRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_

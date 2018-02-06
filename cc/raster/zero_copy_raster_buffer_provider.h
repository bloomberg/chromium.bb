// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace cc {
class LayerTreeResourceProvider;

class CC_EXPORT ZeroCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  ZeroCopyRasterBufferProvider(
      LayerTreeResourceProvider* resource_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      viz::ContextProvider* compositor_context_provider,
      viz::ResourceFormat preferred_tile_format);
  ~ZeroCopyRasterBufferProvider() override;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void OrderingBarrier() override;
  void Flush() override;
  viz::ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool IsResourceSwizzleRequired(bool must_support_alpha) const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) const override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      const base::Closure& callback,
      uint64_t pending_callback_id) const override;
  void Shutdown() override;

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  LayerTreeResourceProvider* resource_provider_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  viz::ContextProvider* compositor_context_provider_;
  viz::ResourceFormat preferred_tile_format_;

  DISALLOW_COPY_AND_ASSIGN(ZeroCopyRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_

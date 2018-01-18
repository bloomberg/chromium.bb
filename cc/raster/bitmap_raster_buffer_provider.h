// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/values.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {
class LayerTreeResourceProvider;

class CC_EXPORT BitmapRasterBufferProvider : public RasterBufferProvider {
 public:
  ~BitmapRasterBufferProvider() override;

  static std::unique_ptr<RasterBufferProvider> Create(
      LayerTreeResourceProvider* resource_provider);

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

 protected:
  explicit BitmapRasterBufferProvider(
      LayerTreeResourceProvider* resource_provider);

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  LayerTreeResourceProvider* resource_provider_;

  DISALLOW_COPY_AND_ASSIGN(BitmapRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_

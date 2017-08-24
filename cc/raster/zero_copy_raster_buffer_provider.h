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

namespace cc {
class LayerTreeResourceProvider;

class CC_EXPORT ZeroCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  ~ZeroCopyRasterBufferProvider() override;

  static std::unique_ptr<RasterBufferProvider> Create(
      LayerTreeResourceProvider* resource_provider,
      viz::ResourceFormat preferred_tile_format);

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override;
  void OrderingBarrier() override;
  void Flush() override;
  viz::ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool IsResourceSwizzleRequired(bool must_support_alpha) const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(viz::ResourceId id) const override;
  uint64_t SetReadyToDrawCallback(
      const ResourceProvider::ResourceIdArray& resource_ids,
      const base::Closure& callback,
      uint64_t pending_callback_id) const override;
  void Shutdown() override;

 protected:
  ZeroCopyRasterBufferProvider(LayerTreeResourceProvider* resource_provider,
                               viz::ResourceFormat preferred_tile_format);

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  LayerTreeResourceProvider* resource_provider_;
  viz::ResourceFormat preferred_tile_format_;

  DISALLOW_COPY_AND_ASSIGN(ZeroCopyRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_

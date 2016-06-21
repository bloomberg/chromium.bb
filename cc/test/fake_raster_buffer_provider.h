// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RASTER_BUFFER_PROVIDER_H_
#define CC_TEST_FAKE_RASTER_BUFFER_PROVIDER_H_

#include "cc/raster/raster_buffer_provider.h"

namespace cc {

// Fake RasterBufferProvider that just no-ops all calls.
class FakeRasterBufferProviderImpl : public RasterBufferProvider {
 public:
  FakeRasterBufferProviderImpl();
  ~FakeRasterBufferProviderImpl() override;

  // RasterBufferProvider methods.
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override;
  void OrderingBarrier() override;
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override;
  void Shutdown() override;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RASTER_BUFFER_PROVIDER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_buffer_provider.h"

namespace cc {

FakeRasterBufferProviderImpl::FakeRasterBufferProviderImpl() {}

FakeRasterBufferProviderImpl::~FakeRasterBufferProviderImpl() {}

std::unique_ptr<RasterBuffer>
FakeRasterBufferProviderImpl::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  return nullptr;
}

void FakeRasterBufferProviderImpl::ReleaseBufferForRaster(
    std::unique_ptr<RasterBuffer> buffer) {}

void FakeRasterBufferProviderImpl::OrderingBarrier() {}

ResourceFormat FakeRasterBufferProviderImpl::GetResourceFormat(
    bool must_support_alpha) const {
  return ResourceFormat::RGBA_8888;
}

bool FakeRasterBufferProviderImpl::GetResourceRequiresSwizzle(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

void FakeRasterBufferProviderImpl::Shutdown() {}

}  // namespace cc

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_hooks.h"

namespace cc {

TestHooks::TestHooks() {}

TestHooks::~TestHooks() {}

DrawResult TestHooks::PrepareToDrawOnThread(
    LayerTreeHostImpl* host_impl,
    LayerTreeHostImpl::FrameData* frame_data,
    DrawResult draw_result) {
  return draw_result;
}

void TestHooks::CreateResourceAndRasterBufferProvider(
    LayerTreeHostImpl* host_impl,
    std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
    std::unique_ptr<ResourcePool>* resource_pool) {
  host_impl->LayerTreeHostImpl::CreateResourceAndRasterBufferProvider(
      raster_buffer_provider, resource_pool);
}

}  // namespace cc

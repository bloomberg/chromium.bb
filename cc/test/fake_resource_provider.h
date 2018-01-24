// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RESOURCE_PROVIDER_H_
#define CC_TEST_FAKE_RESOURCE_PROVIDER_H_

#include "cc/resources/display_resource_provider.h"
#include "cc/resources/layer_tree_resource_provider.h"

namespace cc {

class FakeResourceProvider {
 public:
  static std::unique_ptr<LayerTreeResourceProvider>
  CreateLayerTreeResourceProvider(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr,
      bool high_bit_for_testing = false) {
    viz::ResourceSettings resource_settings;
    resource_settings.high_bit_for_testing = high_bit_for_testing;
    return std::make_unique<LayerTreeResourceProvider>(
        context_provider, shared_bitmap_manager, gpu_memory_buffer_manager,
        true, resource_settings);
  }

  static std::unique_ptr<DisplayResourceProvider> CreateDisplayResourceProvider(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager) {
    return std::make_unique<DisplayResourceProvider>(context_provider,
                                                     shared_bitmap_manager);
  }
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RESOURCE_PROVIDER_H_

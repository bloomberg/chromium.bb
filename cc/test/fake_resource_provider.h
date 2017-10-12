// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RESOURCE_PROVIDER_H_
#define CC_TEST_FAKE_RESOURCE_PROVIDER_H_

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "cc/resources/display_resource_provider.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "cc/resources/resource_provider.h"
#include "components/viz/common/resources/buffer_to_texture_target_map.h"
#include "ui/gfx/buffer_types.h"

namespace cc {

class FakeResourceProvider : public ResourceProvider {
 public:
  static std::unique_ptr<FakeResourceProvider> Create(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      bool high_bit_for_testing = false) {
    viz::ResourceSettings resource_settings;
    resource_settings.texture_id_allocation_chunk_size = 1;
    resource_settings.buffer_to_texture_target_map =
        viz::DefaultBufferToTextureTargetMapForTesting();
    resource_settings.high_bit_for_testing = high_bit_for_testing;
    return base::WrapUnique(
        new FakeResourceProvider(context_provider, shared_bitmap_manager,
                                 nullptr, true, resource_settings));
  }

  static std::unique_ptr<LayerTreeResourceProvider>
  CreateLayerTreeResourceProvider(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr) {
    viz::ResourceSettings resource_settings;
    resource_settings.texture_id_allocation_chunk_size = 1;
    resource_settings.buffer_to_texture_target_map =
        viz::DefaultBufferToTextureTargetMapForTesting();
    return std::make_unique<LayerTreeResourceProvider>(
        context_provider, shared_bitmap_manager, gpu_memory_buffer_manager,
        true, resource_settings);
  }

  static std::unique_ptr<DisplayResourceProvider> CreateDisplayResourceProvider(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr) {
    viz::ResourceSettings resource_settings;
    resource_settings.texture_id_allocation_chunk_size = 1;
    resource_settings.buffer_to_texture_target_map =
        viz::DefaultBufferToTextureTargetMapForTesting();
    return std::make_unique<DisplayResourceProvider>(
        context_provider, shared_bitmap_manager, gpu_memory_buffer_manager,
        resource_settings);
  }

  static std::unique_ptr<FakeResourceProvider> Create(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      bool high_bit_for_testing = false) {
    viz::ResourceSettings resource_settings;
    resource_settings.texture_id_allocation_chunk_size = 1;
    resource_settings.buffer_to_texture_target_map =
        viz::DefaultBufferToTextureTargetMapForTesting();
    resource_settings.high_bit_for_testing = high_bit_for_testing;
    return base::WrapUnique(new FakeResourceProvider(
        context_provider, shared_bitmap_manager, gpu_memory_buffer_manager,
        true, resource_settings));
  }

 private:
  FakeResourceProvider(viz::ContextProvider* context_provider,
                       viz::SharedBitmapManager* shared_bitmap_manager,
                       gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                       bool delegated_sync_points_required,
                       const viz::ResourceSettings resource_settings)
      : ResourceProvider(context_provider,
                         shared_bitmap_manager,
                         gpu_memory_buffer_manager,
                         delegated_sync_points_required,
                         resource_settings) {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RESOURCE_PROVIDER_H_

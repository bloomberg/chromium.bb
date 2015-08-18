// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RESOURCE_PROVIDER_H_
#define CC_TEST_FAKE_RESOURCE_PROVIDER_H_

#include "cc/resources/resource_provider.h"
#include "ui/gfx/buffer_types.h"

namespace cc {

class FakeResourceProvider : public ResourceProvider {
 public:
  static scoped_ptr<FakeResourceProvider> Create(
      OutputSurface* output_surface,
      SharedBitmapManager* shared_bitmap_manager) {
    scoped_ptr<FakeResourceProvider> provider(new FakeResourceProvider(
        output_surface, shared_bitmap_manager, nullptr, nullptr, 0, false, 1,
        false,
        std::vector<unsigned>(static_cast<size_t>(gfx::BufferFormat::LAST) + 1,
                              GL_TEXTURE_2D)));
    provider->Initialize();
    return provider;
  }

  static scoped_ptr<FakeResourceProvider> Create(
      OutputSurface* output_surface,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
    scoped_ptr<FakeResourceProvider> provider(new FakeResourceProvider(
        output_surface, shared_bitmap_manager, gpu_memory_buffer_manager,
        nullptr, 0, false, 1, false,
        std::vector<unsigned>(static_cast<size_t>(gfx::BufferFormat::LAST) + 1,
                              GL_TEXTURE_2D)));
    provider->Initialize();
    return provider;
  }

 private:
  FakeResourceProvider(OutputSurface* output_surface,
                       SharedBitmapManager* shared_bitmap_manager,
                       gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                       BlockingTaskRunner* blocking_main_thread_task_runner,
                       int highp_threshold_min,
                       bool use_rgba_4444_texture_format,
                       size_t id_allocation_chunk_size,
                       bool use_persistent_map_for_gpu_memory_buffers,
                       const std::vector<unsigned>& use_image_texture_targets)
      : ResourceProvider(output_surface,
                         shared_bitmap_manager,
                         gpu_memory_buffer_manager,
                         blocking_main_thread_task_runner,
                         highp_threshold_min,
                         use_rgba_4444_texture_format,
                         id_allocation_chunk_size,
                         use_persistent_map_for_gpu_memory_buffers,
                         use_image_texture_targets) {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RESOURCE_PROVIDER_H_

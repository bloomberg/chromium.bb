// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RESOURCE_PROVIDER_H_
#define CC_TEST_FAKE_RESOURCE_PROVIDER_H_

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/buffer_types.h"

namespace cc {

class FakeResourceProvider : public ResourceProvider {
 public:
  static std::unique_ptr<FakeResourceProvider> Create(
      OutputSurface* output_surface,
      SharedBitmapManager* shared_bitmap_manager) {
    return base::WrapUnique(new FakeResourceProvider(
        output_surface, shared_bitmap_manager, nullptr, nullptr, 0, 1, true,
        false,
        std::vector<unsigned>(static_cast<size_t>(gfx::BufferFormat::LAST) + 1,
                              GL_TEXTURE_2D)));
  }

  static std::unique_ptr<FakeResourceProvider> Create(
      OutputSurface* output_surface,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
    return base::WrapUnique(new FakeResourceProvider(
        output_surface, shared_bitmap_manager, gpu_memory_buffer_manager,
        nullptr, 0, 1, true, false,
        std::vector<unsigned>(static_cast<size_t>(gfx::BufferFormat::LAST) + 1,
                              GL_TEXTURE_2D)));
  }

 private:
  FakeResourceProvider(OutputSurface* output_surface,
                       SharedBitmapManager* shared_bitmap_manager,
                       gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                       BlockingTaskRunner* blocking_main_thread_task_runner,
                       int highp_threshold_min,
                       size_t id_allocation_chunk_size,
                       bool delegated_sync_points_required,
                       bool use_gpu_memory_buffer_resources,
                       const std::vector<unsigned>& use_image_texture_targets)
      : ResourceProvider(output_surface->context_provider(),  // TODO(danakj):
                                                              // Remove output
                                                              // surface dep
                         shared_bitmap_manager,
                         gpu_memory_buffer_manager,
                         blocking_main_thread_task_runner,
                         highp_threshold_min,
                         id_allocation_chunk_size,
                         delegated_sync_points_required,
                         use_gpu_memory_buffer_resources,
                         use_image_texture_targets) {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RESOURCE_PROVIDER_H_

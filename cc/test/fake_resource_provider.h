// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RESOURCE_PROVIDER_H_
#define CC_TEST_FAKE_RESOURCE_PROVIDER_H_

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "cc/output/buffer_to_texture_target_map.h"
#include "cc/output/renderer_settings.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/buffer_types.h"

namespace cc {

class FakeResourceProvider : public ResourceProvider {
 public:
  static std::unique_ptr<FakeResourceProvider> Create(
      ContextProvider* context_provider,
      SharedBitmapManager* shared_bitmap_manager) {
    return base::WrapUnique(new FakeResourceProvider(
        context_provider, shared_bitmap_manager, nullptr, nullptr, 1, true,
        false, false, DefaultBufferToTextureTargetMapForTesting()));
  }

  static std::unique_ptr<FakeResourceProvider> Create(
      ContextProvider* context_provider,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
    return base::WrapUnique(new FakeResourceProvider(
        context_provider, shared_bitmap_manager, gpu_memory_buffer_manager,
        nullptr, 1, true, false, false,
        DefaultBufferToTextureTargetMapForTesting()));
  }

 private:
  FakeResourceProvider(
      ContextProvider* context_provider,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      BlockingTaskRunner* blocking_main_thread_task_runner,
      size_t id_allocation_chunk_size,
      bool delegated_sync_points_required,
      bool use_gpu_memory_buffer_resources,
      bool enable_color_correct_rendering,
      const BufferToTextureTargetMap& buffer_to_texture_target_map)
      : ResourceProvider(context_provider,
                         shared_bitmap_manager,
                         gpu_memory_buffer_manager,
                         blocking_main_thread_task_runner,
                         id_allocation_chunk_size,
                         delegated_sync_points_required,
                         use_gpu_memory_buffer_resources,
                         enable_color_correct_rendering,
                         buffer_to_texture_target_map) {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RESOURCE_PROVIDER_H_

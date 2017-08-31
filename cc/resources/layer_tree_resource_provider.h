// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_LAYER_TREE_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_LAYER_TREE_RESOURCE_PROVIDER_H_

#include "cc/resources/resource_provider.h"

namespace viz {
class SharedBitmapManager;
}  // namespace viz

namespace cc {
class BlockingTaskRunner;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT LayerTreeResourceProvider : public ResourceProvider {
 public:
  LayerTreeResourceProvider(
      viz::ContextProvider* compositor_context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      BlockingTaskRunner* blocking_main_thread_task_runner,
      bool delegated_sync_points_required,
      bool enable_color_correct_rasterization,
      const viz::ResourceSettings& resource_settings);
  ~LayerTreeResourceProvider() override;

  // Gets the most recent sync token from the indicated resources.
  gpu::SyncToken GetSyncTokenForResources(const ResourceIdArray& resource_ids);

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(
      const ResourceIdArray& resource_ids,
      std::vector<viz::TransferableResource>* transferable_resources);

  // Receives resources from the parent, moving them from mailboxes. Resource
  // IDs passed are in the child namespace.
  // NOTE: if the sync_token is set on any viz::TransferableResource, this will
  // wait on it.
  void ReceiveReturnsFromParent(
      const std::vector<viz::ReturnedResource>& transferable_resources);

  // The following lock classes are part of the LayerTreeResourceProvider API
  // and are needed to write the resource contents. The user must ensure that
  // they only use GL locks on GL resources, etc, and this is enforced by
  // assertions.
  class CC_EXPORT ScopedWriteLockGpuMemoryBuffer {
   public:
    ScopedWriteLockGpuMemoryBuffer(LayerTreeResourceProvider* resource_provider,
                                   viz::ResourceId resource_id);
    ~ScopedWriteLockGpuMemoryBuffer();
    gfx::GpuMemoryBuffer* GetGpuMemoryBuffer();
    // Will return the invalid color space unless
    // |enable_color_correct_rasterization| is true.
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

   private:
    LayerTreeResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;

    gfx::Size size_;
    viz::ResourceFormat format_;
    gfx::BufferUsage usage_;
    gfx::ColorSpace color_space_;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGpuMemoryBuffer);
  };

 private:
  void TransferResource(Resource* source,
                        viz::ResourceId id,
                        viz::TransferableResource* resource);

  DISALLOW_COPY_AND_ASSIGN(LayerTreeResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_LAYER_TREE_RESOURCE_PROVIDER_H_

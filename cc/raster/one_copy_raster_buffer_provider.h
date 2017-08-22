// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/staging_buffer_pool.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace cc {
struct StagingBuffer;
class StagingBufferPool;

class CC_EXPORT OneCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  OneCopyRasterBufferProvider(base::SequencedTaskRunner* task_runner,
                              viz::ContextProvider* compositor_context_provider,
                              viz::ContextProvider* worker_context_provider,
                              LayerTreeResourceProvider* resource_provider,
                              int max_copy_texture_chromium_size,
                              bool use_partial_raster,
                              int max_staging_buffer_usage_in_bytes,
                              viz::ResourceFormat preferred_tile_format,
                              bool async_worker_context_enabled);
  ~OneCopyRasterBufferProvider() override;

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

  // Playback raster source and copy result into |resource|.
  void PlaybackAndCopyOnWorkerThread(
      const Resource* resource,
      ResourceProvider::ScopedWriteLockGL* resource_lock,
      const gpu::SyncToken& sync_token,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      const gfx::AxisTransform2d& transform,
      const RasterSource::PlaybackSettings& playback_settings,
      uint64_t previous_content_id,
      uint64_t new_content_id);

 private:
  class RasterBufferImpl : public RasterBuffer {
   public:
    RasterBufferImpl(OneCopyRasterBufferProvider* client,
                     LayerTreeResourceProvider* resource_provider,
                     const Resource* resource,
                     uint64_t previous_content_id);
    ~RasterBufferImpl() override;

    // Overridden from RasterBuffer:
    void Playback(
        const RasterSource* raster_source,
        const gfx::Rect& raster_full_rect,
        const gfx::Rect& raster_dirty_rect,
        uint64_t new_content_id,
        const gfx::AxisTransform2d& transform,
        const RasterSource::PlaybackSettings& playback_settings) override;

    void set_sync_token(const gpu::SyncToken& sync_token) {
      sync_token_ = sync_token;
    }

   private:
    OneCopyRasterBufferProvider* client_;
    const Resource* resource_;
    ResourceProvider::ScopedWriteLockGL lock_;
    uint64_t previous_content_id_;

    gpu::SyncToken sync_token_;

    DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
  };

  void WaitSyncToken(const gpu::SyncToken& sync_token);
  void PlaybackToStagingBuffer(
      StagingBuffer* staging_buffer,
      const Resource* resource,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      const gfx::AxisTransform2d& transform,
      const gfx::ColorSpace& dst_color_space,
      const RasterSource::PlaybackSettings& playback_settings,
      uint64_t previous_content_id,
      uint64_t new_content_id);
  void CopyOnWorkerThread(StagingBuffer* staging_buffer,
                          ResourceProvider::ScopedWriteLockGL* resource_lock,
                          const RasterSource* raster_source,
                          const gfx::Rect& rect_to_copy);
  gfx::BufferUsage StagingBufferUsage() const;

  viz::ContextProvider* const compositor_context_provider_;
  viz::ContextProvider* const worker_context_provider_;
  LayerTreeResourceProvider* const resource_provider_;
  const int max_bytes_per_copy_operation_;
  const bool use_partial_raster_;

  // Context lock must be acquired when accessing this member.
  int bytes_scheduled_since_last_flush_;

  const viz::ResourceFormat preferred_tile_format_;
  StagingBufferPool staging_pool_;

  const bool async_worker_context_enabled_;

  std::set<RasterBufferImpl*> pending_raster_buffers_;

  DISALLOW_COPY_AND_ASSIGN(OneCopyRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_

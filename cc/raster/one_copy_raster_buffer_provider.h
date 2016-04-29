// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/output/context_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/resources/resource_provider.h"

namespace cc {
struct StagingBuffer;
class StagingBufferPool;
class ResourcePool;

class CC_EXPORT OneCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  ~OneCopyRasterBufferProvider() override;

  static std::unique_ptr<RasterBufferProvider> Create(
      base::SequencedTaskRunner* task_runner,
      ContextProvider* context_provider,
      ResourceProvider* resource_provider,
      int max_copy_texture_chromium_size,
      bool use_partial_raster,
      int max_staging_buffer_usage_in_bytes,
      ResourceFormat preferred_tile_format);

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override;
  void OrderingBarrier() override;
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override;
  void Shutdown() override;

  // Playback raster source and copy result into |resource|.
  void PlaybackAndCopyOnWorkerThread(
      const Resource* resource,
      ResourceProvider::ScopedWriteLockGL* resource_lock,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings,
      uint64_t previous_content_id,
      uint64_t new_content_id);

 protected:
  OneCopyRasterBufferProvider(base::SequencedTaskRunner* task_runner,
                              ResourceProvider* resource_provider,
                              int max_copy_texture_chromium_size,
                              bool use_partial_raster,
                              int max_staging_buffer_usage_in_bytes,
                              ResourceFormat preferred_tile_format);

 private:
  void PlaybackToStagingBuffer(
      StagingBuffer* staging_buffer,
      const Resource* resource,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings,
      uint64_t previous_content_id,
      uint64_t new_content_id);
  void CopyOnWorkerThread(StagingBuffer* staging_buffer,
                          const Resource* resource,
                          ResourceProvider::ScopedWriteLockGL* resource_lock,
                          const RasterSource* raster_source,
                          uint64_t previous_content_id,
                          uint64_t new_content_id);

  ResourceProvider* const resource_provider_;
  const int max_bytes_per_copy_operation_;
  bool use_partial_raster_;

  // Context lock must be acquired when accessing this member.
  int bytes_scheduled_since_last_flush_;

  ResourceFormat preferred_tile_format_;
  std::unique_ptr<StagingBufferPool> staging_pool_;

  DISALLOW_COPY_AND_ASSIGN(OneCopyRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_

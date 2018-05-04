// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/raster/raster_buffer_provider.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace viz {
class ContextProvider;
class RasterContextProvider;
}  // namespace viz

namespace cc {

class CC_EXPORT GpuRasterBufferProvider : public RasterBufferProvider {
 public:
  GpuRasterBufferProvider(viz::ContextProvider* compositor_context_provider,
                          viz::RasterContextProvider* worker_context_provider,
                          bool use_gpu_memory_buffer_resources,
                          int gpu_rasterization_msaa_sample_count,
                          viz::ResourceFormat tile_format,
                          const gfx::Size& max_tile_size,
                          bool unpremultiply_and_dither_low_bit_depth_tiles,
                          bool enable_oop_rasterization);
  ~GpuRasterBufferProvider() override;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void Flush() override;
  viz::ResourceFormat GetResourceFormat() const override;
  bool IsResourceSwizzleRequired() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) const override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      const base::Closure& callback,
      uint64_t pending_callback_id) const override;
  void Shutdown() override;

  gpu::SyncToken PlaybackOnWorkerThread(
      const gpu::Mailbox& mailbox,
      GLenum texture_target,
      bool texture_is_overlay_candidate,
      bool texture_storage_allocated,
      const gpu::SyncToken& sync_token,
      const gfx::Size& resource_size,
      viz::ResourceFormat resource_format,
      const gfx::ColorSpace& color_space,
      bool resource_has_previous_content,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      const gfx::AxisTransform2d& transform,
      const RasterSource::PlaybackSettings& playback_settings);

 private:
  class GpuRasterBacking;

  class RasterBufferImpl : public RasterBuffer {
   public:
    RasterBufferImpl(GpuRasterBufferProvider* client,
                     const ResourcePool::InUsePoolResource& in_use_resource,
                     GpuRasterBacking* backing,
                     bool resource_has_previous_content);
    ~RasterBufferImpl() override;

    // Overridden from RasterBuffer:
    void Playback(
        const RasterSource* raster_source,
        const gfx::Rect& raster_full_rect,
        const gfx::Rect& raster_dirty_rect,
        uint64_t new_content_id,
        const gfx::AxisTransform2d& transform,
        const RasterSource::PlaybackSettings& playback_settings) override;

   private:
    // These fields may only be used on the compositor thread.
    GpuRasterBufferProvider* const client_;
    GpuRasterBacking* backing_;

    // These fields are for use on the worker thread.
    const gfx::Size resource_size_;
    const viz::ResourceFormat resource_format_;
    const gfx::ColorSpace color_space_;
    const bool resource_has_previous_content_;
    const gpu::SyncToken before_raster_sync_token_;
    const gpu::Mailbox mailbox_;
    const GLenum texture_target_;
    const bool texture_is_overlay_candidate_;
    // Set to true once allocation is done in the worker thread.
    bool texture_storage_allocated_;
    // A SyncToken to be returned from the worker thread, and waited on before
    // using the rastered resource.
    gpu::SyncToken after_raster_sync_token_;

    DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
  };

  bool ShouldUnpremultiplyAndDitherResource(viz::ResourceFormat format) const;

  viz::ContextProvider* const compositor_context_provider_;
  viz::RasterContextProvider* const worker_context_provider_;
  const bool use_gpu_memory_buffer_resources_;
  const int msaa_sample_count_;
  const viz::ResourceFormat tile_format_;
  const gfx::Size max_tile_size_;
  const bool unpremultiply_and_dither_low_bit_depth_tiles_;
  const bool enable_oop_rasterization_;

  DISALLOW_COPY_AND_ASSIGN(GpuRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_GPU_RASTER_BUFFER_PROVIDER_H_

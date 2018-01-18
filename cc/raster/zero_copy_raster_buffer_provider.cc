// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/zero_copy_raster_buffer_provider.h"

#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "cc/resources/resource.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

class ZeroCopyRasterBufferImpl : public RasterBuffer {
 public:
  ZeroCopyRasterBufferImpl(
      LayerTreeResourceProvider* resource_provider,
      const ResourcePool::InUsePoolResource& in_use_resource)
      : lock_(resource_provider, in_use_resource.gpu_backing_resource_id()),
        resource_size_(in_use_resource.size()),
        resource_format_(in_use_resource.format()) {}

  // Overridden from RasterBuffer:
  void Playback(
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      const gfx::AxisTransform2d& transform,
      const RasterSource::PlaybackSettings& playback_settings) override {
    TRACE_EVENT0("cc", "ZeroCopyRasterBuffer::Playback");
    gfx::GpuMemoryBuffer* buffer = lock_.GetGpuMemoryBuffer();
    if (!buffer)
      return;

    DCHECK_EQ(1u, gfx::NumberOfPlanesForBufferFormat(buffer->GetFormat()));
    bool rv = buffer->Map();
    DCHECK(rv);
    DCHECK(buffer->memory(0));
    // RasterBufferProvider::PlaybackToMemory only supports unsigned strides.
    DCHECK_GE(buffer->stride(0), 0);

    // TODO(danakj): Implement partial raster with raster_dirty_rect.
    RasterBufferProvider::PlaybackToMemory(
        buffer->memory(0), resource_format_, resource_size_, buffer->stride(0),
        raster_source, raster_full_rect, raster_full_rect, transform,
        lock_.color_space_for_raster(), playback_settings);
    buffer->Unmap();
  }

 private:
  LayerTreeResourceProvider::ScopedWriteLockGpuMemoryBuffer lock_;
  gfx::Size resource_size_;
  viz::ResourceFormat resource_format_;

  DISALLOW_COPY_AND_ASSIGN(ZeroCopyRasterBufferImpl);
};

}  // namespace

// static
std::unique_ptr<RasterBufferProvider> ZeroCopyRasterBufferProvider::Create(
    LayerTreeResourceProvider* resource_provider,
    viz::ResourceFormat preferred_tile_format) {
  return base::WrapUnique<RasterBufferProvider>(
      new ZeroCopyRasterBufferProvider(resource_provider,
                                       preferred_tile_format));
}

ZeroCopyRasterBufferProvider::ZeroCopyRasterBufferProvider(
    LayerTreeResourceProvider* resource_provider,
    viz::ResourceFormat preferred_tile_format)
    : resource_provider_(resource_provider),
      preferred_tile_format_(preferred_tile_format) {}

ZeroCopyRasterBufferProvider::~ZeroCopyRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
ZeroCopyRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  return std::make_unique<ZeroCopyRasterBufferImpl>(resource_provider_,
                                                    resource);
}

void ZeroCopyRasterBufferProvider::OrderingBarrier() {
  // No need to sync resources as this provider does not use GL context.
}

void ZeroCopyRasterBufferProvider::Flush() {}

viz::ResourceFormat ZeroCopyRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  if (resource_provider_->IsTextureFormatSupported(preferred_tile_format_) &&
      (DoesResourceFormatSupportAlpha(preferred_tile_format_) ||
       !must_support_alpha)) {
    return preferred_tile_format_;
  }

  return resource_provider_->best_texture_format();
}

bool ZeroCopyRasterBufferProvider::IsResourceSwizzleRequired(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

bool ZeroCopyRasterBufferProvider::CanPartialRasterIntoProvidedResource()
    const {
  return false;
}

bool ZeroCopyRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  // Zero-copy resources are immediately ready to draw.
  return true;
}

uint64_t ZeroCopyRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
  // Zero-copy resources are immediately ready to draw.
  return 0;
}

void ZeroCopyRasterBufferProvider::Shutdown() {}

}  // namespace cc

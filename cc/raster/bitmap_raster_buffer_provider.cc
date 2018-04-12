// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/bitmap_raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/raster/raster_source.h"
#include "cc/resources/resource.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"

namespace cc {
namespace {

class BitmapSoftwareBacking : public ResourcePool::SoftwareBacking {
 public:
  ~BitmapSoftwareBacking() override {
    frame_sink->DidDeleteSharedBitmap(shared_bitmap_id);
  }

  base::UnguessableToken SharedMemoryGuid() override {
    return shared_memory->mapped_id();
  }

  LayerTreeFrameSink* frame_sink;
  std::unique_ptr<base::SharedMemory> shared_memory;
};

class BitmapRasterBufferImpl : public RasterBuffer {
 public:
  BitmapRasterBufferImpl(const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         void* pixels,
                         uint64_t resource_content_id,
                         uint64_t previous_content_id)
      : resource_size_(size),
        color_space_(color_space),
        pixels_(pixels),
        resource_has_previous_content_(
            resource_content_id && resource_content_id == previous_content_id) {
  }

  // Overridden from RasterBuffer:
  void Playback(
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      const gfx::AxisTransform2d& transform,
      const RasterSource::PlaybackSettings& playback_settings) override {
    TRACE_EVENT0("cc", "BitmapRasterBuffer::Playback");
    gfx::Rect playback_rect = raster_full_rect;
    if (resource_has_previous_content_) {
      playback_rect.Intersect(raster_dirty_rect);
    }
    DCHECK(!playback_rect.IsEmpty())
        << "Why are we rastering a tile that's not dirty?";

    size_t stride = 0u;
    RasterBufferProvider::PlaybackToMemory(
        pixels_, viz::RGBA_8888, resource_size_, stride, raster_source,
        raster_full_rect, playback_rect, transform, color_space_,
        playback_settings);
  }

 private:
  const gfx::Size resource_size_;
  const gfx::ColorSpace color_space_;
  void* const pixels_;
  bool resource_has_previous_content_;

  DISALLOW_COPY_AND_ASSIGN(BitmapRasterBufferImpl);
};

}  // namespace

BitmapRasterBufferProvider::BitmapRasterBufferProvider(
    LayerTreeFrameSink* frame_sink)
    : frame_sink_(frame_sink) {}

BitmapRasterBufferProvider::~BitmapRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
BitmapRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  DCHECK_EQ(resource.format(), viz::RGBA_8888);

  const gfx::Size& size = resource.size();
  const gfx::ColorSpace& color_space = resource.color_space();
  if (!resource.software_backing()) {
    auto backing = std::make_unique<BitmapSoftwareBacking>();
    backing->frame_sink = frame_sink_;
    backing->shared_bitmap_id = viz::SharedBitmap::GenerateId();
    backing->shared_memory =
        viz::bitmap_allocation::AllocateMappedBitmap(size, viz::RGBA_8888);

    mojo::ScopedSharedBufferHandle handle =
        viz::bitmap_allocation::DuplicateAndCloseMappedBitmap(
            backing->shared_memory.get(), size, viz::RGBA_8888);
    frame_sink_->DidAllocateSharedBitmap(std::move(handle),
                                         backing->shared_bitmap_id);

    resource.set_software_backing(std::move(backing));
  }
  BitmapSoftwareBacking* backing =
      static_cast<BitmapSoftwareBacking*>(resource.software_backing());

  return std::make_unique<BitmapRasterBufferImpl>(
      size, color_space, backing->shared_memory->memory(), resource_content_id,
      previous_content_id);
}

void BitmapRasterBufferProvider::Flush() {}

viz::ResourceFormat BitmapRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  return viz::RGBA_8888;
}

bool BitmapRasterBufferProvider::IsResourceSwizzleRequired(
    bool must_support_alpha) const {
  // GetResourceFormat() returns a constant so use it directly.
  return ResourceFormatRequiresSwizzle(viz::RGBA_8888);
}

bool BitmapRasterBufferProvider::IsResourcePremultiplied(
    bool must_support_alpha) const {
  return true;
}

bool BitmapRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  return true;
}

bool BitmapRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  // Bitmap resources are immediately ready to draw.
  return true;
}

uint64_t BitmapRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
  // Bitmap resources are immediately ready to draw.
  return 0;
}

void BitmapRasterBufferProvider::Shutdown() {}

}  // namespace cc

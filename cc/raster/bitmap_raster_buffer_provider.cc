// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/bitmap_raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/raster/raster_source.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "cc/resources/resource.h"
#include "components/viz/common/resources/platform_color.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(LayerTreeResourceProvider* resource_provider,
                   const Resource* resource,
                   uint64_t resource_content_id,
                   uint64_t previous_content_id)
      : lock_(resource_provider, resource->id()),
        resource_(resource),
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
        lock_.sk_bitmap().getPixels(), resource_->format(), resource_->size(),
        stride, raster_source, raster_full_rect, playback_rect, transform,
        lock_.color_space_for_raster(), playback_settings);
  }

 private:
  ResourceProvider::ScopedWriteLockSoftware lock_;
  const Resource* resource_;
  bool resource_has_previous_content_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

}  // namespace

// static
std::unique_ptr<RasterBufferProvider> BitmapRasterBufferProvider::Create(
    LayerTreeResourceProvider* resource_provider) {
  return base::WrapUnique<RasterBufferProvider>(
      new BitmapRasterBufferProvider(resource_provider));
}

BitmapRasterBufferProvider::BitmapRasterBufferProvider(
    LayerTreeResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {}

BitmapRasterBufferProvider::~BitmapRasterBufferProvider() {}

std::unique_ptr<RasterBuffer>
BitmapRasterBufferProvider::AcquireBufferForRaster(
    const Resource* resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id) {
  return std::unique_ptr<RasterBuffer>(new RasterBufferImpl(
      resource_provider_, resource, resource_content_id, previous_content_id));
}

void BitmapRasterBufferProvider::ReleaseBufferForRaster(
    std::unique_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void BitmapRasterBufferProvider::OrderingBarrier() {
  // No need to sync resources as this provider does not use GL context.
}

void BitmapRasterBufferProvider::Flush() {}

viz::ResourceFormat BitmapRasterBufferProvider::GetResourceFormat(
    bool must_support_alpha) const {
  return resource_provider_->best_texture_format();
}

bool BitmapRasterBufferProvider::IsResourceSwizzleRequired(
    bool must_support_alpha) const {
  return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
}

bool BitmapRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  return true;
}

bool BitmapRasterBufferProvider::IsResourceReadyToDraw(
    viz::ResourceId resource_id) const {
  // Bitmap resources are immediately ready to draw.
  return true;
}

uint64_t BitmapRasterBufferProvider::SetReadyToDrawCallback(
    const ResourceProvider::ResourceIdArray& resource_ids,
    const base::Closure& callback,
    uint64_t pending_callback_id) const {
  // Bitmap resources are immediately ready to draw.
  return 0;
}

void BitmapRasterBufferProvider::Shutdown() {}

}  // namespace cc

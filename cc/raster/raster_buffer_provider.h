// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_RASTER_BUFFER_PROVIDER_H_

#include <stddef.h>

#include "cc/raster/raster_buffer.h"
#include "cc/raster/raster_source.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/tile_task.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class Resource;

class CC_EXPORT RasterBufferProvider {
 public:
  RasterBufferProvider();
  virtual ~RasterBufferProvider();

  // Utility function that will create a temporary bitmap and copy pixels to
  // |memory| when necessary. The |canvas_bitmap_rect| is the rect of the bitmap
  // being played back in the pixel space of the source, ie a rect in the source
  // that will cover the resulting |memory|. The |canvas_playback_rect| can be a
  // smaller contained rect inside the |canvas_bitmap_rect| if the |memory| is
  // already partially complete, and only the subrect needs to be played back.
  static void PlaybackToMemory(
      void* memory,
      ResourceFormat format,
      const gfx::Size& size,
      size_t stride,
      const RasterSource* raster_source,
      const gfx::Rect& canvas_bitmap_rect,
      const gfx::Rect& canvas_playback_rect,
      const gfx::AxisTransform2d& transform,
      const gfx::ColorSpace& target_color_space,
      const RasterSource::PlaybackSettings& playback_settings);

  // Acquire raster buffer.
  virtual std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) = 0;

  // Release raster buffer.
  virtual void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) = 0;

  // Used for syncing resources to the worker context.
  virtual void OrderingBarrier() = 0;

  // In addition to above, also ensures that pending work is sent to the GPU
  // process.
  virtual void Flush() = 0;

  // Returns the format to use for the tiles.
  virtual ResourceFormat GetResourceFormat(bool must_support_alpha) const = 0;

  // Determine if the resource requires swizzling.
  virtual bool IsResourceSwizzleRequired(bool must_support_alpha) const = 0;

  // Determine if the RasterBufferProvider can handle partial raster into
  // the Resource provided in AcquireBufferForRaster.
  virtual bool CanPartialRasterIntoProvidedResource() const = 0;

  // Returns true if the indicated resource is ready to draw.
  virtual bool IsResourceReadyToDraw(ResourceId id) const = 0;

  // Calls the provided |callback| when the provided |resources| are ready to
  // draw. Returns a callback ID which can be used to track this callback.
  // Will return 0 if no callback is needed (resources are already ready to
  // draw). The caller may optionally pass the ID of a pending callback to
  // avoid creating a new callback unnecessarily. If the caller does not
  // have a pending callback, 0 should be passed for |pending_callback_id|.
  virtual uint64_t SetReadyToDrawCallback(
      const ResourceProvider::ResourceIdArray& resource_ids,
      const base::Callback<void()>& callback,
      uint64_t pending_callback_id) const = 0;

  // Shutdown for doing cleanup.
  virtual void Shutdown() = 0;

 protected:
  // Check if resource format matches output format.
  static bool ResourceFormatRequiresSwizzle(ResourceFormat format);
};

}  // namespace cc

#endif  // CC_RASTER_RASTER_BUFFER_PROVIDER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_RASTER_BUFFER_H_
#define CC_RASTER_RASTER_BUFFER_H_

#include <stdint.h>

#include "cc/base/cc_export.h"
#include "cc/playback/raster_source.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class RasterBuffer;
class Resource;

class CC_EXPORT RasterBufferProvider {
 public:
  virtual std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) = 0;
  virtual void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) = 0;

 protected:
  virtual ~RasterBufferProvider() {}
};

class CC_EXPORT RasterBuffer {
 public:
  RasterBuffer();
  virtual ~RasterBuffer();

  virtual void Playback(
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      uint64_t new_content_id,
      float scale,
      const RasterSource::PlaybackSettings& playback_settings) = 0;
};

}  // namespace cc

#endif  // CC_RASTER_RASTER_BUFFER_H_

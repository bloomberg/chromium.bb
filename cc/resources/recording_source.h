// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RECORDING_SOURCE_H_
#define CC_RESOURCES_RECORDING_SOURCE_H_

#include "cc/base/cc_export.h"
#include "cc/resources/picture.h"
#include "third_party/skia/include/core/SkBBHFactory.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class ContentLayerClient;
class Region;
class RasterSource;

class CC_EXPORT RecordingSource {
 public:
  virtual ~RecordingSource() {}
  // Re-record parts of the picture that are invalid.
  // Invalidations are in layer space, and will be expanded to cover everything
  // that was either recorded/changed or that has no recording, leaving out only
  // pieces that we had a recording for and it was not changed.
  // Return true iff the pile was modified.
  virtual bool UpdateAndExpandInvalidation(
      ContentLayerClient* painter,
      Region* invalidation,
      SkColor background_color,
      bool contents_opaque,
      bool contents_fill_bounds_completely,
      const gfx::Size& layer_size,
      const gfx::Rect& visible_layer_rect,
      int frame_number,
      Picture::RecordingMode recording_mode) = 0;

  virtual gfx::Size GetSize() const = 0;
  virtual void SetEmptyBounds() = 0;
  virtual void SetMinContentsScale(float min_contents_scale) = 0;
  virtual void SetTileGridSize(const gfx::Size& tile_grid_size) = 0;
  virtual void SetSlowdownRasterScaleFactor(int factor) = 0;
  virtual void SetIsMask(bool is_mask) = 0;
  virtual bool IsSuitableForGpuRasterization() const = 0;

  virtual scoped_refptr<RasterSource> CreateRasterSource() const = 0;

  // TODO(hendrikw): Figure out how to remove this.
  virtual void SetUnsuitableForGpuRasterizationForTesting() = 0;
  virtual SkTileGridFactory::TileGridInfo GetTileGridInfoForTesting() const = 0;
};
}

#endif  // CC_RESOURCES_RECORDING_SOURCE_H_

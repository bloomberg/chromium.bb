// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_PAINT_WORKLET_IMAGE_CACHE_H_
#define CC_TILES_PAINT_WORKLET_IMAGE_CACHE_H_

#include "cc/cc_export.h"
#include "cc/paint/draw_image.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/image_decode_cache.h"

namespace cc {

// PaintWorkletImageCache is responsible for generating tasks of executing
// PaintWorklet JS paint callbacks, and being able to return the generated
// results when requested.
class CC_EXPORT PaintWorkletImageCache {
 public:
  PaintWorkletImageCache();

  ~PaintWorkletImageCache();

  scoped_refptr<TileTask> GetTaskForPaintWorkletImage(const DrawImage& image);

  void PaintImageInTask(const PaintImage& paint_image);
};

}  // namespace cc

#endif  // CC_TILES_PAINT_WORKLET_IMAGE_CACHE_H_

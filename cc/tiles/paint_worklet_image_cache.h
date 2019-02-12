// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_PAINT_WORKLET_IMAGE_CACHE_H_
#define CC_TILES_PAINT_WORKLET_IMAGE_CACHE_H_

#include <utility>

#include "base/containers/flat_map.h"
#include "cc/cc_export.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/image_decode_cache.h"

namespace cc {

// PaintWorkletImageCache is responsible for generating tasks of executing
// PaintWorklet JS paint callbacks, and being able to return the generated
// results when requested.
class CC_EXPORT PaintWorkletImageCache {
 public:
  struct CC_EXPORT PaintWorkletImageCacheValue {
    PaintWorkletImageCacheValue();
    PaintWorkletImageCacheValue(sk_sp<PaintRecord> record, size_t ref_count);
    PaintWorkletImageCacheValue(const PaintWorkletImageCacheValue&);
    ~PaintWorkletImageCacheValue();

    sk_sp<PaintRecord> record;
    size_t used_ref_count;
  };

  PaintWorkletImageCache();

  ~PaintWorkletImageCache();

  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter);

  scoped_refptr<TileTask> GetTaskForPaintWorkletImage(const DrawImage& image);

  void PaintImageInTask(const PaintImage& paint_image);

  // Returns a callback to decrement the ref count for the corresponding entry.
  std::pair<PaintRecord*, base::OnceCallback<void()>> GetPaintRecordAndRef(
      PaintWorkletInput* input);

  const base::flat_map<PaintWorkletInput*, PaintWorkletImageCacheValue>&
  GetRecordsForTest() {
    return records_;
  }

 private:
  void DecrementCacheRefCount(PaintWorkletInput* input);
  // This is a map of paint worklet inputs to a pair of paint record and a
  // reference count. The paint record is the representation of the worklet
  // output based on the input, and the reference count is the number of times
  // that it is used for tile rasterization.
  // TODO(xidachen): use a struct instead of std::pair.
  base::flat_map<PaintWorkletInput*, PaintWorkletImageCacheValue> records_;
  // The PaintWorkletImageCache is owned by ImageController, which has the same
  // life time as the LayerTreeHostImpl, that guarantees that the painter will
  // live as long as the LayerTreeHostImpl.
  std::unique_ptr<PaintWorkletLayerPainter> painter_;
};

}  // namespace cc

#endif  // CC_TILES_PAINT_WORKLET_IMAGE_CACHE_H_

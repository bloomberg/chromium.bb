// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CONTROLLER_H_
#define CC_TILES_IMAGE_DECODE_CONTROLLER_H_

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/raster/tile_task_runner.h"
#include "skia/ext/pixel_ref_utils.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace cc {

class ImageDecodeController {
 public:
  ImageDecodeController();
  ~ImageDecodeController();

  scoped_refptr<ImageDecodeTask> GetTaskForPixelRef(
      const skia::PositionPixelRef& pixel_ref,
      int layer_id,
      uint64_t prepare_tiles_id);

  // Note that this function has to remain thread safe.
  void DecodePixelRef(SkPixelRef* pixel_ref);

  // TODO(vmpstr): This should go away once the controller is decoding images
  // based on priority and memory.
  void AddLayerUsedCount(int layer_id);
  void SubtractLayerUsedCount(int layer_id);

  void OnImageDecodeTaskCompleted(int layer_id,
                                  SkPixelRef* pixel_ref,
                                  bool was_canceled);

 private:
  scoped_refptr<ImageDecodeTask> CreateTaskForPixelRef(
      SkPixelRef* pixel_ref,
      int layer_id,
      uint64_t prepare_tiles_id);

  using PixelRefTaskMap =
      base::hash_map<uint32_t, scoped_refptr<ImageDecodeTask>>;
  using LayerPixelRefTaskMap = base::hash_map<int, PixelRefTaskMap>;
  LayerPixelRefTaskMap image_decode_tasks_;

  using LayerCountMap = base::hash_map<int, int>;
  LayerCountMap used_layer_counts_;
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CONTROLLER_H_

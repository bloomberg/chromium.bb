// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_MANAGER_H_
#define CC_TILES_IMAGE_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/playback/draw_image.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/image_decode_controller.h"

namespace cc {

class CC_EXPORT ImageManager {
 public:
  ImageManager();
  ~ImageManager();

  void SetImageDecodeController(ImageDecodeController* controller);
  void GetTasksForImagesAndRef(
      std::vector<DrawImage>* images,
      std::vector<scoped_refptr<TileTask>>* tasks,
      const ImageDecodeController::TracingInfo& tracing_info);
  void UnrefImages(const std::vector<DrawImage>& images);
  void ReduceMemoryUsage();

 private:
  ImageDecodeController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ImageManager);
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_MANAGER_H_

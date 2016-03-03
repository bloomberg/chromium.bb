// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_
#define CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_

#include <unordered_map>
#include <unordered_set>

#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/tiles/image_decode_controller.h"

namespace cc {

class CC_EXPORT GpuImageDecodeController : public ImageDecodeController {
 public:
  GpuImageDecodeController();
  ~GpuImageDecodeController() override;

  // ImageDecodeController overrides.
  bool GetTaskForImageAndRef(const DrawImage& image,
                             uint64_t prepare_tiles_id,
                             scoped_refptr<ImageDecodeTask>* task) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& draw_image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;

  void DecodeImage(const DrawImage& image);

  void RemovePendingTaskForImage(const DrawImage& image);

 private:
  base::Lock lock_;

  std::unordered_set<uint32_t> prerolled_images_;
  std::unordered_map<uint32_t, scoped_refptr<ImageDecodeTask>>
      pending_image_tasks_;
};

}  // namespace cc

#endif  // CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_

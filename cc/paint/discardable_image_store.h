// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DISCARDABLE_IMAGE_STORE_H_
#define CC_PAINT_DISCARDABLE_IMAGE_STORE_H_

#include "base/containers/flat_map.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {
class PaintFlags;
class PaintImage;

class CC_PAINT_EXPORT DiscardableImageStore {
 public:
  DiscardableImageStore(int width,
                        int height,
                        std::vector<std::pair<DrawImage, gfx::Rect>>* image_set,
                        base::flat_map<ImageId, gfx::Rect>* image_id_to_rect);
  ~DiscardableImageStore();

  void GatherDiscardableImages(const PaintOpBuffer* buffer);
  SkNoDrawCanvas* GetNoDrawCanvas();

 private:
  class PaintTrackingCanvas;

  void AddImageFromFlags(const SkRect& rect, const PaintFlags& flags);
  void AddImage(const PaintImage& paint_image,
                const SkRect& src_rect,
                const SkRect& rect,
                const SkMatrix* local_matrix,
                const PaintFlags& flags);

  // This canvas is used only for tracking transform/clip/filter state from the
  // non-drawing ops.
  std::unique_ptr<PaintTrackingCanvas> canvas_;
  std::vector<std::pair<DrawImage, gfx::Rect>>* image_set_;
  base::flat_map<ImageId, gfx::Rect>* image_id_to_rect_;
};

}  // namespace cc

#endif  // CC_PAINT_DISCARDABLE_IMAGE_STORE_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DRAW_IMAGE_H_
#define CC_PLAYBACK_DRAW_IMAGE_H_

#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace cc {

class DrawImage {
 public:
  DrawImage() : image_(nullptr), filter_quality_(kNone_SkFilterQuality) {}
  DrawImage(const SkImage* image,
            const SkMatrix& matrix,
            SkFilterQuality filter_quality)
      : image_(image), matrix_(matrix), filter_quality_(filter_quality) {}

  const SkImage* image() const { return image_; }
  const SkMatrix& matrix() const { return matrix_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }

  DrawImage ApplyScale(float scale) {
    SkMatrix scaled_matrix = matrix_;
    scaled_matrix.preScale(scale, scale);
    return DrawImage(image_, scaled_matrix, filter_quality_);
  }

 private:
  const SkImage* image_;
  SkMatrix matrix_;
  SkFilterQuality filter_quality_;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DRAW_IMAGE_H_

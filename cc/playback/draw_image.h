// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DRAW_IMAGE_H_
#define CC_PLAYBACK_DRAW_IMAGE_H_

#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace cc {

// TODO(vmpstr): This should probably be DISALLOW_COPY_AND_ASSIGN and transport
// it around using a pointer, since it became kind of large. Profile.
class CC_EXPORT DrawImage {
 public:
  DrawImage();
  DrawImage(const SkImage* image,
            const SkIRect& src_rect,
            const SkSize& scale,
            SkFilterQuality filter_quality,
            bool matrix_has_perspective,
            bool matrix_is_decomposable);
  DrawImage(const DrawImage& other);

  const SkImage* image() const { return image_; }
  const SkSize& scale() const { return scale_; }
  const SkIRect src_rect() const { return src_rect_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  bool matrix_has_perspective() const { return matrix_has_perspective_; }
  bool matrix_is_decomposable() const { return matrix_is_decomposable_; }

  DrawImage ApplyScale(float scale) const {
    return DrawImage(
        image_, src_rect_,
        SkSize::Make(scale_.width() * scale, scale_.height() * scale),
        filter_quality_, matrix_has_perspective_, matrix_is_decomposable_);
  }

 private:
  const SkImage* image_;
  SkIRect src_rect_;
  SkSize scale_;
  SkFilterQuality filter_quality_;
  bool matrix_has_perspective_;
  bool matrix_is_decomposable_;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DRAW_IMAGE_H_

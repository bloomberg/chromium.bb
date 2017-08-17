// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DRAW_IMAGE_H_
#define CC_PAINT_DRAW_IMAGE_H_

#include "base/optional.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

// TODO(vmpstr): This should probably be DISALLOW_COPY_AND_ASSIGN and transport
// it around using a pointer, since it became kind of large. Profile.
class CC_PAINT_EXPORT DrawImage {
 public:
  DrawImage();
  DrawImage(PaintImage image,
            const SkIRect& src_rect,
            SkFilterQuality filter_quality,
            const SkMatrix& matrix,
            const base::Optional<gfx::ColorSpace>& color_space = base::nullopt);
  // Constructs a DrawImage from |other| by adjusting its scale and setting a
  // new color_space.
  DrawImage(const DrawImage& other,
            float scale_adjustment,
            const gfx::ColorSpace& color_space);
  DrawImage(const DrawImage& other);
  DrawImage(DrawImage&& other);
  ~DrawImage();

  DrawImage& operator=(DrawImage&& other);
  DrawImage& operator=(const DrawImage& other);

  bool operator==(const DrawImage& other) const;

  const PaintImage& paint_image() const { return paint_image_; }
  const sk_sp<SkImage>& image() const { return paint_image_.GetSkImage(); }
  const SkSize& scale() const { return scale_; }
  const SkIRect& src_rect() const { return src_rect_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  bool matrix_is_decomposable() const { return matrix_is_decomposable_; }
  const SkMatrix& matrix() const { return matrix_; }
  const gfx::ColorSpace& target_color_space() const {
    DCHECK(target_color_space_.has_value());
    return *target_color_space_;
  }

 private:
  PaintImage paint_image_;
  SkIRect src_rect_;
  SkFilterQuality filter_quality_;
  SkMatrix matrix_;
  SkSize scale_;
  bool matrix_is_decomposable_;
  base::Optional<gfx::ColorSpace> target_color_space_;
};

}  // namespace cc

#endif  // CC_PAINT_DRAW_IMAGE_H_

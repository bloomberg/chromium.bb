// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_PROVIDER_H_
#define CC_PAINT_IMAGE_PROVIDER_H_

#include "cc/paint/paint_export.h"

namespace cc {
class DecodedDrawImage;
class PaintImage;

// Used to replace lazy generated PaintImages with decoded images to use for
// rasterization.
class CC_PAINT_EXPORT ImageProvider {
 public:
  class DecodedImageHolder {
   public:
    virtual ~DecodedImageHolder() {}
    virtual const DecodedDrawImage& DecodedImage() = 0;
  };

  virtual ~ImageProvider() {}

  virtual std::unique_ptr<DecodedImageHolder> GetDecodedImage(
      const PaintImage& paint_image,
      const SkRect& src_rect,
      SkFilterQuality filter_quality,
      const SkMatrix& matrix) = 0;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_PROVIDER_H_

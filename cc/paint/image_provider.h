// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_PROVIDER_H_
#define CC_PAINT_IMAGE_PROVIDER_H_

#include "base/callback.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/paint_export.h"

namespace cc {
class PaintImage;

// Used to replace lazy generated PaintImages with decoded images for
// rasterization.
class CC_PAINT_EXPORT ImageProvider {
 public:
  class CC_PAINT_EXPORT ScopedDecodedDrawImage {
   public:
    using DestructionCallback = base::OnceCallback<void(DecodedDrawImage)>;

    ScopedDecodedDrawImage();
    explicit ScopedDecodedDrawImage(DecodedDrawImage image);
    ScopedDecodedDrawImage(DecodedDrawImage image,
                           DestructionCallback callback);
    ~ScopedDecodedDrawImage();

    ScopedDecodedDrawImage(ScopedDecodedDrawImage&& other);
    ScopedDecodedDrawImage& operator=(ScopedDecodedDrawImage&& other);

    operator bool() const { return image_.image(); }
    const DecodedDrawImage& decoded_image() const { return image_; }

   private:
    void DestroyDecode();

    DecodedDrawImage image_;
    DestructionCallback destruction_callback_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDecodedDrawImage);
  };

  virtual ~ImageProvider() {}

  // Returns the DecodedDrawImage to use for this PaintImage. If no image is
  // provided, the draw for this image will be skipped during raster.
  virtual ScopedDecodedDrawImage GetDecodedDrawImage(
      const PaintImage& paint_image,
      const SkRect& src_rect,
      SkFilterQuality filter_quality,
      const SkMatrix& matrix) = 0;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_PROVIDER_H_

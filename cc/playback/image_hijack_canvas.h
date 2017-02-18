// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_IMAGE_HIJACK_CANVAS_H_
#define CC_PLAYBACK_IMAGE_HIJACK_CANVAS_H_

#include <unordered_set>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/playback/image_id.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

namespace cc {

class ImageDecodeCache;

class CC_EXPORT ImageHijackCanvas : public SkNWayCanvas {
 public:
  ImageHijackCanvas(int width,
                    int height,
                    ImageDecodeCache* image_decode_cache,
                    const ImageIdFlatSet* images_to_skip);

 private:
  // Ensure that pictures are unpacked by this canvas, instead of being
  // forwarded to the raster canvas.
  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override;
  void onDrawImage(const SkImage* image,
                   SkScalar x,
                   SkScalar y,
                   const SkPaint* paint) override;
  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint constraint) override;
  void onDrawRect(const SkRect&, const SkPaint&) override;
  void onDrawPath(const SkPath& path, const SkPaint& paint) override;
  void onDrawOval(const SkRect& r, const SkPaint& paint) override;
  void onDrawArc(const SkRect& r,
                 SkScalar start_angle,
                 SkScalar sweep_angle,
                 bool use_center,
                 const SkPaint& paint) override;
  void onDrawRRect(const SkRRect& rr, const SkPaint& paint) override;
  void onDrawImageNine(const SkImage* image,
                       const SkIRect& center,
                       const SkRect& dst,
                       const SkPaint* paint) override;

  bool ShouldSkipImage(const SkImage* image) const;
  bool ShouldSkipImageInPaint(const SkPaint& paint) const;

  ImageDecodeCache* image_decode_cache_;
  const ImageIdFlatSet* images_to_skip_;

  DISALLOW_COPY_AND_ASSIGN(ImageHijackCanvas);
};

}  // namespace cc

#endif  // CC_PLAYBACK_IMAGE_HIJACK_CANVAS_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_GENERATOR_H_
#define CC_PAINT_PAINT_IMAGE_GENERATOR_H_

#include "base/macros.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkYUVSizeInfo.h"

namespace cc {

// PaintImage Generator is a wrapper to provide a lazily decoded PaintImage to
// the compositor.
// Note that the implementation of this class must ensure thread safety, it can
// be called from any thread.
class CC_PAINT_EXPORT PaintImageGenerator : public SkRefCnt {
 public:
  // Returns a reference to the encoded content of this image.
  virtual sk_sp<SkData> GetEncodedData() const = 0;

  // Decode into the given pixels, a block of memory of size at least
  // (info.fHeight - 1) * rowBytes + (info.fWidth *  bytesPerPixel). |info|
  // represents the desired output format. Returns true on success.
  //
  // TODO(khushalsagar): |lazy_pixel_ref| is only present for
  // DecodingImageGenerator tracing needs. Remove it.
  virtual bool GetPixels(const SkImageInfo& info,
                         void* pixels,
                         size_t row_bytes,
                         uint32_t lazy_pixel_ref) = 0;

  // Returns true if the generator supports YUV decoding, providing the output
  // information in |info| and |color_space|.
  virtual bool QueryYUV8(SkYUVSizeInfo* info,
                         SkYUVColorSpace* color_space) const = 0;

  // Decodes to YUV into the provided |planes| for each of the Y, U, and V
  // planes, and returns true on success. The method should only be used if
  // QueryYUV8 returns true.
  // |info| needs to exactly match the values returned by the query, except the
  // WidthBytes may be larger than the recommendation (but not smaller).
  //
  // TODO(khushalsagar): |lazy_pixel_ref| is only present for
  // DecodingImageGenerator tracing needs. Remove it.
  virtual bool GetYUV8Planes(const SkYUVSizeInfo& info,
                             void* planes[3],
                             uint32_t lazy_pixel_ref) = 0;

  const SkImageInfo& GetSkImageInfo() const { return info_; }

 protected:
  explicit PaintImageGenerator(const SkImageInfo& info) : info_(info) {}

 private:
  const SkImageInfo info_;

  DISALLOW_COPY_AND_ASSIGN(PaintImageGenerator);
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_GENERATOR_H_

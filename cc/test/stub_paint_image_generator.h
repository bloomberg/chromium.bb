// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_PAINT_IMAGE_GENERATOR_H_
#define CC_TEST_STUB_PAINT_IMAGE_GENERATOR_H_

#include "cc/paint/paint_image_generator.h"

namespace cc {

class StubPaintImageGenerator : public PaintImageGenerator {
 public:
  explicit StubPaintImageGenerator(const SkImageInfo& info)
      : PaintImageGenerator(info) {}

  sk_sp<SkData> GetEncodedData() const override;
  bool GetPixels(const SkImageInfo& info,
                 void* pixels,
                 size_t row_bytes,
                 uint32_t lazy_pixel_ref) override;
  bool QueryYUV8(SkYUVSizeInfo* info,
                 SkYUVColorSpace* color_space) const override;
  bool GetYUV8Planes(const SkYUVSizeInfo& info,
                     void* planes[3],
                     uint32_t lazy_pixel_ref) override;
};

}  // namespace cc

#endif  // CC_TEST_STUB_PAINT_IMAGE_GENERATOR_H_

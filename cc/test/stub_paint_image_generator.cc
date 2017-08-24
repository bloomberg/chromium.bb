// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/stub_paint_image_generator.h"

namespace cc {

sk_sp<SkData> StubPaintImageGenerator::GetEncodedData() const {
  return nullptr;
}

bool StubPaintImageGenerator::GetPixels(const SkImageInfo& info,
                                        void* pixels,
                                        size_t row_bytes,
                                        uint32_t lazy_pixel_ref) {
  return false;
}

bool StubPaintImageGenerator::QueryYUV8(SkYUVSizeInfo* info,
                                        SkYUVColorSpace* color_space) const {
  return false;
}

bool StubPaintImageGenerator::GetYUV8Planes(const SkYUVSizeInfo& info,
                                            void* planes[3],
                                            uint32_t lazy_pixel_ref) {
  return false;
}

}  // namespace cc

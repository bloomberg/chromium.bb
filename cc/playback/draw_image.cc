// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/draw_image.h"

namespace cc {

DrawImage::DrawImage()
    : image_(nullptr),
      src_rect_(SkIRect::MakeXYWH(0, 0, 0, 0)),
      scale_(SkSize::Make(1.f, 1.f)),
      filter_quality_(kNone_SkFilterQuality) {}

DrawImage::DrawImage(const SkImage* image,
                     const SkIRect& src_rect,
                     const SkSize& scale,
                     SkFilterQuality filter_quality,
                     bool matrix_has_perspective,
                     bool matrix_is_decomposable)
    : image_(image),
      src_rect_(src_rect),
      scale_(scale),
      filter_quality_(filter_quality),
      matrix_has_perspective_(matrix_has_perspective),
      matrix_is_decomposable_(matrix_is_decomposable) {}

DrawImage::DrawImage(const DrawImage& other)
    : image_(other.image_),
      src_rect_(other.src_rect_),
      scale_(other.scale_),
      filter_quality_(other.filter_quality_),
      matrix_has_perspective_(other.matrix_has_perspective_),
      matrix_is_decomposable_(other.matrix_is_decomposable_) {}

}  // namespace cc

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/utility/cloud_print/bitmap_image.h"

namespace cloud_print {

namespace {
const uint8_t kCurrentlySupportedNumberOfChannels = 4;
}

BitmapImage::BitmapImage(const gfx::Size& size, Colorspace colorspace)
    : size_(size),
      colorspace_(colorspace),
      data_(new uint8_t[size.GetArea() * channels()]) {}

BitmapImage::~BitmapImage() {
}

uint8_t BitmapImage::channels() const {
  return kCurrentlySupportedNumberOfChannels;
}

const uint8_t* BitmapImage::GetPixel(const gfx::Point& point) const {
  DCHECK_LT(point.x(), size_.width());
  DCHECK_LT(point.y(), size_.height());
  return data_.get() + (point.y() * size_.width() + point.x()) * channels();
}

}  // namespace cloud_print

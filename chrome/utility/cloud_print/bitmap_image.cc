// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/cloud_print/bitmap_image.h"

namespace cloud_print {

namespace {
const uint8 kCurrentlySupportedNumberOfChannels = 4;
}

BitmapImage::BitmapImage(const gfx::Size& size,
                         Colorspace colorspace)
    : size_(size),
      colorspace_(colorspace),
      data_(new uint8[size.GetArea() * channels()]) {
}

BitmapImage::~BitmapImage() {
}

uint8 BitmapImage::channels() const {
  return kCurrentlySupportedNumberOfChannels;
}

}  // namespace cloud_print

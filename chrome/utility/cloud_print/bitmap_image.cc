// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/cloud_print/bitmap_image.h"

namespace printing {

namespace {
const uint8 kCurrentlySupportedNumberOfChannels = 4;
}

BitmapImage::BitmapImage(int32 width,
                         int32 height,
                         Colorspace colorspace)
    : width_(width),
      height_(height),
      colorspace_(colorspace),
      data_(new uint8[width * height * channels()]) {
}

BitmapImage::~BitmapImage() {
}

uint8 BitmapImage::channels() const {
  return kCurrentlySupportedNumberOfChannels;
}

}  // namespace printing

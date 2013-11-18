// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CLOUD_PRINT_BITMAP_IMAGE_H_
#define CHROME_UTILITY_CLOUD_PRINT_BITMAP_IMAGE_H_

#include "base/memory/scoped_ptr.h"

namespace cloud_print {

class BitmapImage {
 public:
  enum Colorspace {
    // These are the only types PWGEncoder currently supports.
    RGBA,
    BGRA
  };

  BitmapImage(int32 width,
              int32 height,
              Colorspace colorspace);
  ~BitmapImage();

  uint8 channels() const;
  int32 width() const { return width_; }
  int32 height() const { return height_; }
  Colorspace colorspace() const { return colorspace_; }

  const uint8* pixel_data() const { return data_.get(); }
  uint8* pixel_data() { return data_.get(); }

 private:
  int32 width_;
  int32 height_;
  Colorspace colorspace_;
  scoped_ptr<uint8[]> data_;

  DISALLOW_COPY_AND_ASSIGN(BitmapImage);
};

}  // namespace cloud_print

#endif  // CHROME_UTILITY_CLOUD_PRINT_BITMAP_IMAGE_H_

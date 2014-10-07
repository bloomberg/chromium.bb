// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_IMAGE_ENCODER_H_
#define COMPONENTS_SUGGESTIONS_IMAGE_ENCODER_H_

#include <vector>

#include "base/macros.h"

class SkBitmap;

namespace suggestions {

// A interface to encode/decode images.
class ImageEncoder {
 public:
  ImageEncoder() {}
  virtual ~ImageEncoder() {}

  // From encoded bytes to SkBitmap. Caller owns the returned pointer.
  virtual SkBitmap* DecodeImage(
      const std::vector<unsigned char>& encoded_data) = 0;

  // From SkBitmap to the vector of encoded bytes, |dst|.
  virtual bool EncodeImage(
      const SkBitmap& bitmap, std::vector<unsigned char>* dest) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageEncoder);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_IMAGE_ENCODER_H_

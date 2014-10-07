// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_JPEG_JPEG_IMAGE_ENCODER_H_
#define COMPONENTS_SUGGESTIONS_JPEG_JPEG_IMAGE_ENCODER_H_

#include <vector>

#include "base/macros.h"
#include "components/suggestions/image_encoder.h"

class SkBitmap;

namespace suggestions {

// A class used to encode/decode images using the JPEG codec.
class JpegImageEncoder : public ImageEncoder {
 public:
  JpegImageEncoder() {}
  virtual ~JpegImageEncoder() {}

  // From encoded bytes to SkBitmap. Caller owns the returned pointer.
  virtual SkBitmap* DecodeImage(
      const std::vector<unsigned char>& encoded_data) OVERRIDE;

  // From SkBitmap to the vector of encoded bytes, |dst|.
  virtual bool EncodeImage(const SkBitmap& bitmap,
                           std::vector<unsigned char>* dest) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(JpegImageEncoder);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_JPEG_JPEG_IMAGE_ENCODER_H_

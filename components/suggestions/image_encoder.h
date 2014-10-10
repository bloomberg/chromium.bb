// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_IMAGE_ENCODER_H_
#define COMPONENTS_SUGGESTIONS_IMAGE_ENCODER_H_

#include <vector>

class SkBitmap;

namespace suggestions {

// From encoded bytes to SkBitmap. It's the caller's responsibility to delete
// the bitmap.
SkBitmap* DecodeJPEGToSkBitmap(const std::vector<unsigned char>& encoded_data);

// From SkBitmap to a vector of JPEG-encoded bytes, |dst|.
bool EncodeSkBitmapToJPEG(const SkBitmap& bitmap,
                          std::vector<unsigned char>* dest);

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_IMAGE_ENCODER_H_

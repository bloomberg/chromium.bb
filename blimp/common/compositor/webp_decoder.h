// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_COMPOSITOR_WEBP_DECODER_H_
#define BLIMP_COMMON_COMPOSITOR_WEBP_DECODER_H_

#include <stddef.h>

#include "blimp/common/blimp_common_export.h"
#include "third_party/skia/include/core/SkData.h"

class SkBitmap;

namespace blimp {

// WebPDecoder is an implementation of SkPicture::InstallPixelRefProc which
// is used by the client to decode WebP images that are part of an SkPicture.
BLIMP_COMMON_EXPORT bool WebPDecoder(const void* input,
                                     size_t input_size,
                                     SkBitmap* bitmap);

}  // namespace blimp

#endif  // BLIMP_COMMON_COMPOSITOR_WEBP_DECODER_H_

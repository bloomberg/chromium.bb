// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the YUV conversion functions for each specific
// optimization.

#ifndef MEDIA_BASE_YUV_CONVERT_INTERNAL_H_
#define MEDIA_BASE_YUV_CONVERT_INTERNAL_H_

#include "base/basictypes.h"

namespace media {

// SSE2 version of converting RGBA to YV12.
extern void ConvertRGB32ToYUV_SSE2(const uint8* rgbframe,
                                   uint8* yplane,
                                   uint8* uplane,
                                   uint8* vplane,
                                   int width,
                                   int height,
                                   int rgbstride,
                                   int ystride,
                                   int uvstride);

// C version of converting RGBA to YV12.
void ConvertRGB32ToYUV_C(const uint8* rgbframe,
                         uint8* yplane,
                         uint8* uplane,
                         uint8* vplane,
                         int width,
                         int height,
                         int rgbstride,
                         int ystride,
                         int uvstride);

}  // namespace media

#endif  // MEDIA_BASE_YUV_CONVERT_INTERNAL_H_

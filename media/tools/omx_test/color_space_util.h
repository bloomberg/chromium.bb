// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Color space conversion methods, they are for testing purpose and are
// not optimized for production use.

#ifndef MEDIA_TOOLS_OMX_TEST_COLOR_SPACE_UTIL_H_
#define MEDIA_TOOLS_OMX_TEST_COLOR_SPACE_UTIL_H_

#include "base/basictypes.h"

namespace media {

// First parameter is the input buffer, second parameter is the output
// buffer.
void NV21toIYUV(const uint8* nv21, uint8* iyuv, int width, int height);
void NV21toYV12(const uint8* nv21, uint8* yv12, int width, int height);
void IYUVtoNV21(const uint8* iyuv, uint8* nv21, int width, int height);
void YV12toNV21(const uint8* yv12, uint8* nv21, int width, int height);

}  // namespace media

#endif  // MEDIA_TOOLS_OMX_TEST_COLOR_SPACE_UTIL_H_

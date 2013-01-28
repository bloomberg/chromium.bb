// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_DIFFER_BLOCK_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_DIFFER_BLOCK_H_

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

// Size (in pixels) of each square block used for diffing. This must be a
// multiple of sizeof(uint64)/8.
const int kBlockSize = 32;

// Format: BGRA 32 bit.
const int kBytesPerPixel = 4;

// Low level functions to compare 2 blocks of pixels. Zero means the blocks
// are identical. One - the blocks are different.
MEDIA_EXPORT int BlockDifference(const uint8* image1,
                                 const uint8* image2,
                                 int stride);

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_DIFFER_BLOCK_H_

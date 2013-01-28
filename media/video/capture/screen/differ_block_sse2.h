// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file is used only differ_block.h. It defines the SSE2 rountines
// for finding block difference.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_DIFFER_BLOCK_SSE2_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_DIFFER_BLOCK_SSE2_H_

#include "base/basictypes.h"

namespace media {

// Find block difference of dimension 16x16.
extern int BlockDifference_SSE2_W16(const uint8* image1, const uint8* image2,
                                    int stride);

// Find block difference of dimension 32x32.
extern int BlockDifference_SSE2_W32(const uint8* image1, const uint8* image2,
                                    int stride);

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_DIFFER_BLOCK_SSE2_H_

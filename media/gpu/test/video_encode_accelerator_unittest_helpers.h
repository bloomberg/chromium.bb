// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODE_ACCELERATOR_UNITTEST_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODE_ACCELERATOR_UNITTEST_HELPERS_H_

#include "media/base/video_frame.h"

namespace media {
namespace test {

// Create DMABuf-backed video frame from MEM-backed VideoFrame. The created
// VideoFrame owns the buffer resource.
scoped_refptr<VideoFrame> CreateDmabufFrameFromVideoFrame(
    scoped_refptr<VideoFrame> frame);

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_ENCODE_ACCELERATOR_UNITTEST_HELPERS_H_

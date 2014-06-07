// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_decode_accelerator.h"

namespace media {

VideoDecodeAccelerator::~VideoDecodeAccelerator() {}

}

namespace base {

void DefaultDeleter<media::VideoDecodeAccelerator>::operator()(
    void* video_decode_accelerator) const {
  static_cast<media::VideoDecodeAccelerator*>(video_decode_accelerator)->
      Destroy();
}

}  // namespace base



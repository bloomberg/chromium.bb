// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/video_decode_accelerator_impl.h"

namespace content {

VideoDecodeAcceleratorImpl::VideoDecodeAcceleratorImpl() {}

VideoDecodeAcceleratorImpl::~VideoDecodeAcceleratorImpl() {}

bool VideoDecodeAcceleratorImpl::CanDecodeOnIOThread() { return false; }

}  // namespace content

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/compositor_frame_metadata.h"

namespace cc {

CompositorFrameMetadata::CompositorFrameMetadata()
    : device_scale_factor(0),
      page_scale_factor(0),
      min_page_scale_factor(0),
      max_page_scale_factor(0) {
}

CompositorFrameMetadata::~CompositorFrameMetadata() {
}

}  // namespace cc

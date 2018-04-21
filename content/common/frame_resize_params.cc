// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_resize_params.h"

namespace content {

FrameResizeParams::FrameResizeParams() = default;
FrameResizeParams::FrameResizeParams(const FrameResizeParams& other) = default;
FrameResizeParams::~FrameResizeParams() = default;

FrameResizeParams& FrameResizeParams::operator=(
    const FrameResizeParams& other) = default;

}  // namespace content

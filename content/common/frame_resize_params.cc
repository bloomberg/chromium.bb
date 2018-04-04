// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_resize_params.h"

namespace content {

FrameResizeParams::FrameResizeParams()
    : auto_resize_enabled(false), auto_resize_sequence_number(0u) {}

FrameResizeParams::FrameResizeParams(const FrameResizeParams& other) = default;

FrameResizeParams::~FrameResizeParams() {}

FrameResizeParams& FrameResizeParams::operator=(
    const FrameResizeParams& other) = default;

}  // namespace content

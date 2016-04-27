// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/buffer_presented_params_mac.h"

namespace content {

BufferPresentedParams::BufferPresentedParams()
    : surface_handle(gpu::kNullSurfaceHandle) {}

BufferPresentedParams::~BufferPresentedParams() {}

}  // namespace content

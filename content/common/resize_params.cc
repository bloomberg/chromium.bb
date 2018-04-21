// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resize_params.h"

namespace content {

ResizeParams::ResizeParams() = default;
ResizeParams::ResizeParams(const ResizeParams& other) = default;
ResizeParams::~ResizeParams() = default;

ResizeParams& ResizeParams::operator=(const ResizeParams& other) = default;

}  // namespace content

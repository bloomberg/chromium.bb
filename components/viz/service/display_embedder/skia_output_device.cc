// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device.h"

namespace viz {

SkiaOutputDevice::SkiaOutputDevice() = default;

SkiaOutputDevice::~SkiaOutputDevice() = default;

bool SkiaOutputDevice::SupportPostSubBuffer() {
  return false;
}

gfx::SwapResult SkiaOutputDevice::PostSubBuffer(const gfx::Rect& rect) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

}  // namespace viz

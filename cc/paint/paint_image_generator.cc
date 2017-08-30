// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image_generator.h"

#include <vector>

#include "base/logging.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {

SkISize PaintImageGenerator::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  // The base class just returns the original size as the only supported decode
  // size.
  return info_.dimensions();
}

}  // namespace cc

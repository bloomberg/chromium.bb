// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_ID_H_
#define CC_PAINT_IMAGE_ID_H_

#include <stdint.h>
#include <unordered_set>

#include "base/containers/flat_set.h"
#include "cc/paint/paint_image.h"

namespace cc {

using PaintImageIdFlatSet = base::flat_set<PaintImage::Id>;

// TODO(khushalsagar): These are only used by the hijack canvas since it uses
// an SkCanvas to replace images. Remove once that moves to PaintOpBuffer.
using SkImageId = uint32_t;
using SkImageIdFlatSet = base::flat_set<SkImageId>;

}  // namespace cc

#endif  // CC_PAINT_IMAGE_ID_H_

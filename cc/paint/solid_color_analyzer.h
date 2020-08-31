// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SOLID_COLOR_ANALYZER_H_
#define CC_PAINT_SOLID_COLOR_ANALYZER_H_

#include <vector>

#include "base/optional.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/skia_util.h"

namespace cc {

class CC_PAINT_EXPORT SolidColorAnalyzer {
 public:
  SolidColorAnalyzer() = delete;

  static base::Optional<SkColor> DetermineIfSolidColor(
      const PaintOpBuffer* buffer,
      const gfx::Rect& rect,
      int max_ops_to_analyze,
      const std::vector<size_t>* offsets = nullptr);
};

}  // namespace cc

#endif  // CC_PAINT_SOLID_COLOR_ANALYZER_H_

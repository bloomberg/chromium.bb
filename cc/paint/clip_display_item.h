// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_CLIP_DISPLAY_ITEM_H_
#define CC_PAINT_CLIP_DISPLAY_ITEM_H_

#include <stddef.h>

#include <vector>

#include "cc/paint/display_item.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class CC_PAINT_EXPORT ClipDisplayItem : public DisplayItem {
 public:
  ClipDisplayItem(const gfx::Rect& clip_rect,
                  std::vector<SkRRect> rounded_clip_rects,
                  bool antialias);
  ~ClipDisplayItem() override;

  size_t ExternalMemoryUsage() const {
    return rounded_clip_rects.capacity() * sizeof(rounded_clip_rects[0]);
  }
  int ApproximateOpCount() const { return 1; }

  const gfx::Rect clip_rect;
  const std::vector<SkRRect> rounded_clip_rects;
  const bool antialias;
};

class CC_PAINT_EXPORT EndClipDisplayItem : public DisplayItem {
 public:
  EndClipDisplayItem();
  ~EndClipDisplayItem() override;

  int ApproximateOpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PAINT_CLIP_DISPLAY_ITEM_H_

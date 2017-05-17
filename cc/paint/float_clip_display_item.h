// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_FLOAT_CLIP_DISPLAY_ITEM_H_
#define CC_PAINT_FLOAT_CLIP_DISPLAY_ITEM_H_

#include <stddef.h>

#include "cc/paint/display_item.h"
#include "cc/paint/paint_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

class CC_PAINT_EXPORT FloatClipDisplayItem : public DisplayItem {
 public:
  explicit FloatClipDisplayItem(const gfx::RectF& clip_rect);
  ~FloatClipDisplayItem() override;

  size_t ExternalMemoryUsage() const { return 0; }
  int OpCount() const { return 1; }

  const gfx::RectF clip_rect;
};

class CC_PAINT_EXPORT EndFloatClipDisplayItem : public DisplayItem {
 public:
  EndFloatClipDisplayItem();
  ~EndFloatClipDisplayItem() override;

  int OpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PAINT_FLOAT_CLIP_DISPLAY_ITEM_H_

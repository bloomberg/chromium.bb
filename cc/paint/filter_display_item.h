// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_FILTER_DISPLAY_ITEM_H_
#define CC_PAINT_FILTER_DISPLAY_ITEM_H_

#include "cc/base/filter_operations.h"
#include "cc/paint/display_item.h"
#include "cc/paint/paint_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

class CC_PAINT_EXPORT FilterDisplayItem : public DisplayItem {
 public:
  FilterDisplayItem(const FilterOperations& filters,
                    const gfx::RectF& bounds,
                    const gfx::PointF& origin);
  ~FilterDisplayItem() override;

  size_t ExternalMemoryUsage() const {
    // FilterOperations doesn't expose its capacity, but size is probably good
    // enough.
    return filters.size() * sizeof(filters.at(0));
  }
  int OpCount() const { return 1; }

  const FilterOperations filters;
  const gfx::RectF bounds;
  const gfx::PointF origin;
};

class CC_PAINT_EXPORT EndFilterDisplayItem : public DisplayItem {
 public:
  EndFilterDisplayItem();
  ~EndFilterDisplayItem() override;

  int OpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PAINT_FILTER_DISPLAY_ITEM_H_

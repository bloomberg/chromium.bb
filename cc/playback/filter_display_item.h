// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_
#define CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_

#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "cc/playback/display_item.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

class CC_EXPORT FilterDisplayItem : public DisplayItem {
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
  int ApproximateOpCount() const { return 1; }

  const FilterOperations filters;
  const gfx::RectF bounds;
  const gfx::PointF origin;
};

class CC_EXPORT EndFilterDisplayItem : public DisplayItem {
 public:
  EndFilterDisplayItem();
  ~EndFilterDisplayItem() override;

  int ApproximateOpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_FILTER_DISPLAY_ITEM_H_
#define CC_RESOURCES_FILTER_DISPLAY_ITEM_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "cc/resources/display_item.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT FilterDisplayItem : public DisplayItem {
 public:
  ~FilterDisplayItem() override;

  static scoped_ptr<FilterDisplayItem> Create(const FilterOperations& filters,
                                              gfx::RectF bounds) {
    return make_scoped_ptr(new FilterDisplayItem(filters, bounds));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  FilterDisplayItem(const FilterOperations& filters, gfx::RectF bounds);

 private:
  FilterOperations filters_;
  gfx::RectF bounds_;
};

class CC_EXPORT EndFilterDisplayItem : public DisplayItem {
 public:
  ~EndFilterDisplayItem() override;

  static scoped_ptr<EndFilterDisplayItem> Create() {
    return make_scoped_ptr(new EndFilterDisplayItem());
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  EndFilterDisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_FILTER_DISPLAY_ITEM_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TRANSFORM_DISPLAY_ITEM_H_
#define CC_RESOURCES_TRANSFORM_DISPLAY_ITEM_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/display_item.h"
#include "ui/gfx/transform.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT TransformDisplayItem : public DisplayItem {
 public:
  ~TransformDisplayItem() override;

  static scoped_ptr<TransformDisplayItem> Create(
      const gfx::Transform& transform) {
    return make_scoped_ptr(new TransformDisplayItem(transform));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  explicit TransformDisplayItem(const gfx::Transform& transform);

 private:
  gfx::Transform transform_;
};

class CC_EXPORT EndTransformDisplayItem : public DisplayItem {
 public:
  ~EndTransformDisplayItem() override;

  static scoped_ptr<EndTransformDisplayItem> Create() {
    return make_scoped_ptr(new EndTransformDisplayItem());
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  EndTransformDisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_TRANSFORM_DISPLAY_ITEM_H_

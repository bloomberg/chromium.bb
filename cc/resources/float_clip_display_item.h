// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_FLOAT_CLIP_DISPLAY_ITEM_H_
#define CC_RESOURCES_FLOAT_CLIP_DISPLAY_ITEM_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/display_item.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT FloatClipDisplayItem : public DisplayItem {
 public:
  ~FloatClipDisplayItem() override;

  static scoped_ptr<FloatClipDisplayItem> Create(gfx::RectF clip_rect) {
    return make_scoped_ptr(new FloatClipDisplayItem(clip_rect));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  explicit FloatClipDisplayItem(gfx::RectF clip_rect);

 private:
  gfx::RectF clip_rect_;
};

class CC_EXPORT EndFloatClipDisplayItem : public DisplayItem {
 public:
  ~EndFloatClipDisplayItem() override;

  static scoped_ptr<EndFloatClipDisplayItem> Create() {
    return make_scoped_ptr(new EndFloatClipDisplayItem());
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  EndFloatClipDisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_FLOAT_CLIP_DISPLAY_ITEM_H_

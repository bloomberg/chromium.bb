// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DRAWING_DISPLAY_ITEM_H_
#define CC_RESOURCES_DRAWING_DISPLAY_ITEM_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/display_item.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/geometry/point_f.h"

class SkCanvas;
class SkDrawPictureCallback;
class SkPicture;

namespace cc {

class CC_EXPORT DrawingDisplayItem : public DisplayItem {
 public:
  ~DrawingDisplayItem() override;

  static scoped_ptr<DrawingDisplayItem> Create(
      skia::RefPtr<SkPicture> picture) {
    return make_scoped_ptr(new DrawingDisplayItem(picture));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;
  void RasterForTracing(SkCanvas* canvas) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  explicit DrawingDisplayItem(skia::RefPtr<SkPicture> picture);

 private:
  skia::RefPtr<SkPicture> picture_;
};

}  // namespace cc

#endif  // CC_RESOURCES_DRAWING_DISPLAY_ITEM_H_

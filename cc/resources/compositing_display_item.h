// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_COMPOSITING_DISPLAY_ITEM_H_
#define CC_RESOURCES_COMPOSITING_DISPLAY_ITEM_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/display_item.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT CompositingDisplayItem : public DisplayItem {
 public:
  ~CompositingDisplayItem() override;

  static scoped_ptr<CompositingDisplayItem> Create(
      float opacity,
      SkXfermode::Mode xfermode,
      SkRect* bounds,
      skia::RefPtr<SkColorFilter> color_filter) {
    return make_scoped_ptr(
        new CompositingDisplayItem(opacity, xfermode, bounds, color_filter));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  CompositingDisplayItem(float opacity,
                         SkXfermode::Mode,
                         SkRect* bounds,
                         skia::RefPtr<SkColorFilter>);

 private:
  float opacity_;
  SkXfermode::Mode xfermode_;
  bool has_bounds_;
  SkRect bounds_;
  skia::RefPtr<SkColorFilter> color_filter_;
};

class CC_EXPORT EndCompositingDisplayItem : public DisplayItem {
 public:
  ~EndCompositingDisplayItem() override;

  static scoped_ptr<EndCompositingDisplayItem> Create() {
    return make_scoped_ptr(new EndCompositingDisplayItem());
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  EndCompositingDisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_COMPOSITING_DISPLAY_ITEM_H_

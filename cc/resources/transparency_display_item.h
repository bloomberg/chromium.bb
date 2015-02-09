// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TRANSPARENCY_DISPLAY_ITEM_H_
#define CC_RESOURCES_TRANSPARENCY_DISPLAY_ITEM_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/display_item.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT TransparencyDisplayItem : public DisplayItem {
 public:
  ~TransparencyDisplayItem() override;

  static scoped_ptr<TransparencyDisplayItem> Create(
      float opacity,
      SkXfermode::Mode blend_mode) {
    return make_scoped_ptr(new TransparencyDisplayItem(opacity, blend_mode));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  TransparencyDisplayItem(float opacity, SkXfermode::Mode blend_mode);

 private:
  float opacity_;
  SkXfermode::Mode blend_mode_;
};

class CC_EXPORT EndTransparencyDisplayItem : public DisplayItem {
 public:
  ~EndTransparencyDisplayItem() override;

  static scoped_ptr<EndTransparencyDisplayItem> Create() {
    return make_scoped_ptr(new EndTransparencyDisplayItem());
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  EndTransparencyDisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_TRANSPARENCY_DISPLAY_ITEM_H_

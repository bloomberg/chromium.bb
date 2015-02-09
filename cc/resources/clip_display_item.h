// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_CLIP_DISPLAY_ITEM_H_
#define CC_RESOURCES_CLIP_DISPLAY_ITEM_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/display_item.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT ClipDisplayItem : public DisplayItem {
 public:
  ~ClipDisplayItem() override;

  static scoped_ptr<ClipDisplayItem> Create(
      gfx::Rect clip_rect,
      const std::vector<SkRRect>& rounded_clip_rects) {
    return make_scoped_ptr(new ClipDisplayItem(clip_rect, rounded_clip_rects));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  ClipDisplayItem(gfx::Rect clip_rect,
                  const std::vector<SkRRect>& rounded_clip_rects);

 private:
  gfx::Rect clip_rect_;
  std::vector<SkRRect> rounded_clip_rects_;
};

class CC_EXPORT EndClipDisplayItem : public DisplayItem {
 public:
  ~EndClipDisplayItem() override;

  static scoped_ptr<EndClipDisplayItem> Create() {
    return make_scoped_ptr(new EndClipDisplayItem());
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  EndClipDisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_CLIP_DISPLAY_ITEM_H_

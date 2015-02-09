// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_CLIP_PATH_DISPLAY_ITEM_H_
#define CC_RESOURCES_CLIP_PATH_DISPLAY_ITEM_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/resources/display_item.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"

class SkCanvas;
class SkDrawPictureCallback;

namespace cc {

class CC_EXPORT ClipPathDisplayItem : public DisplayItem {
 public:
  ~ClipPathDisplayItem() override;

  static scoped_ptr<ClipPathDisplayItem> Create(const SkPath& path,
                                                SkRegion::Op clip_op,
                                                bool antialias) {
    return make_scoped_ptr(new ClipPathDisplayItem(path, clip_op, antialias));
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  ClipPathDisplayItem(const SkPath& path, SkRegion::Op clip_op, bool antialias);

 private:
  SkPath clip_path_;
  SkRegion::Op clip_op_;
  bool antialias_;
};

class CC_EXPORT EndClipPathDisplayItem : public DisplayItem {
 public:
  ~EndClipPathDisplayItem() override;

  static scoped_ptr<EndClipPathDisplayItem> Create() {
    return make_scoped_ptr(new EndClipPathDisplayItem());
  }

  void Raster(SkCanvas* canvas, SkDrawPictureCallback* callback) const override;

  bool IsSuitableForGpuRasterization() const override;
  int ApproximateOpCount() const override;
  size_t PictureMemoryUsage() const override;
  void AsValueInto(base::trace_event::TracedValue* array) const override;

 protected:
  EndClipPathDisplayItem();
};

}  // namespace cc

#endif  // CC_RESOURCES_CLIP_PATH_DISPLAY_ITEM_H_

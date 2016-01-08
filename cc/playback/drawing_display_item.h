// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DRAWING_DISPLAY_ITEM_H_
#define CC_PLAYBACK_DRAWING_DISPLAY_ITEM_H_

#include <stddef.h>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/playback/display_item.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/geometry/point_f.h"

class SkCanvas;
class SkPicture;

namespace cc {

class CC_EXPORT DrawingDisplayItem : public DisplayItem {
 public:
  DrawingDisplayItem();
  explicit DrawingDisplayItem(skia::RefPtr<const SkPicture> picture);
  explicit DrawingDisplayItem(const proto::DisplayItem& proto);
  explicit DrawingDisplayItem(const DrawingDisplayItem& item);
  ~DrawingDisplayItem() override;

  void ToProtobuf(proto::DisplayItem* proto) const override;
  void Raster(SkCanvas* canvas,
              const gfx::Rect& canvas_playback_rect,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const;
  bool IsSuitableForGpuRasterization() const;

  void CloneTo(DrawingDisplayItem* item) const;

 private:
  void SetNew(skia::RefPtr<const SkPicture> picture);

  skia::RefPtr<const SkPicture> picture_;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DRAWING_DISPLAY_ITEM_H_

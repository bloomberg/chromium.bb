// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_
#define CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_

#include <stddef.h>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/filter_operations.h"
#include "cc/playback/display_item.h"
#include "ui/gfx/geometry/rect_f.h"

class SkCanvas;

namespace cc {
class ImageSerializationProcessor;

class CC_EXPORT FilterDisplayItem : public DisplayItem {
 public:
  FilterDisplayItem(const FilterOperations& filters, const gfx::RectF& bounds);
  explicit FilterDisplayItem(const proto::DisplayItem& proto);
  ~FilterDisplayItem() override;

  void ToProtobuf(proto::DisplayItem* proto,
                  ImageSerializationProcessor* image_serialization_processor)
      const override;
  void Raster(SkCanvas* canvas,
              const gfx::Rect& canvas_target_playback_rect,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const { return 1; }
  bool IsSuitableForGpuRasterization() const { return true; }

 private:
  void SetNew(const FilterOperations& filters, const gfx::RectF& bounds);

  FilterOperations filters_;
  gfx::RectF bounds_;
};

class CC_EXPORT EndFilterDisplayItem : public DisplayItem {
 public:
  EndFilterDisplayItem();
  explicit EndFilterDisplayItem(const proto::DisplayItem& proto);
  ~EndFilterDisplayItem() override;

  static scoped_ptr<EndFilterDisplayItem> Create() {
    return make_scoped_ptr(new EndFilterDisplayItem());
  }

  void ToProtobuf(proto::DisplayItem* proto,
                  ImageSerializationProcessor* image_serialization_processor)
      const override;
  void Raster(SkCanvas* canvas,
              const gfx::Rect& canvas_target_playback_rect,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;
  size_t ExternalMemoryUsage() const override;

  int ApproximateOpCount() const { return 0; }
  bool IsSuitableForGpuRasterization() const { return true; }
};

}  // namespace cc

#endif  // CC_PLAYBACK_FILTER_DISPLAY_ITEM_H_

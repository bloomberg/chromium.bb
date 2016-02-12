// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_CLIP_DISPLAY_ITEM_H_
#define CC_PLAYBACK_CLIP_DISPLAY_ITEM_H_

#include <stddef.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/playback/display_item.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect.h"

class SkCanvas;

namespace cc {
class ImageSerializationProcessor;

class CC_EXPORT ClipDisplayItem : public DisplayItem {
 public:
  ClipDisplayItem(const gfx::Rect& clip_rect,
                  const std::vector<SkRRect>& rounded_clip_rects);
  explicit ClipDisplayItem(const proto::DisplayItem& proto);
  ~ClipDisplayItem() override;

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
  void SetNew(const gfx::Rect& clip_rect,
              const std::vector<SkRRect>& rounded_clip_rects);

  gfx::Rect clip_rect_;
  std::vector<SkRRect> rounded_clip_rects_;
};

class CC_EXPORT EndClipDisplayItem : public DisplayItem {
 public:
  EndClipDisplayItem();
  explicit EndClipDisplayItem(const proto::DisplayItem& proto);
  ~EndClipDisplayItem() override;

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

#endif  // CC_PLAYBACK_CLIP_DISPLAY_ITEM_H_

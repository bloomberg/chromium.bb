// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_CLIP_PATH_DISPLAY_ITEM_H_
#define CC_PLAYBACK_CLIP_PATH_DISPLAY_ITEM_H_

#include <stddef.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/playback/display_item.h"
#include "third_party/skia/include/core/SkPath.h"

class SkCanvas;

namespace cc {

class CC_EXPORT ClipPathDisplayItem : public DisplayItem {
 public:
  ClipPathDisplayItem(const SkPath& path, bool antialias);
  ~ClipPathDisplayItem() override;

  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;

  size_t ExternalMemoryUsage() const {
    // The size of SkPath's external storage is not currently accounted for (and
    // may well be shared anyway).
    return 0;
  }
  int ApproximateOpCount() const { return 1; }

 private:
  void SetNew(const SkPath& path, bool antialias);

  SkPath clip_path_;
  bool antialias_;
};

class CC_EXPORT EndClipPathDisplayItem : public DisplayItem {
 public:
  EndClipPathDisplayItem();
  ~EndClipPathDisplayItem() override;

  static std::unique_ptr<EndClipPathDisplayItem> Create() {
    return base::MakeUnique<EndClipPathDisplayItem>();
  }

  void Raster(SkCanvas* canvas,
              SkPicture::AbortCallback* callback) const override;
  void AsValueInto(const gfx::Rect& visual_rect,
                   base::trace_event::TracedValue* array) const override;

  int ApproximateOpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PLAYBACK_CLIP_PATH_DISPLAY_ITEM_H_

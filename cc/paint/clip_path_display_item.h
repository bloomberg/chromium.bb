// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_CLIP_PATH_DISPLAY_ITEM_H_
#define CC_PAINT_CLIP_PATH_DISPLAY_ITEM_H_

#include <stddef.h>

#include "cc/paint/display_item.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkPath.h"

namespace cc {

class CC_PAINT_EXPORT ClipPathDisplayItem : public DisplayItem {
 public:
  ClipPathDisplayItem(const SkPath& path, bool antialias);
  ~ClipPathDisplayItem() override;

  size_t ExternalMemoryUsage() const {
    // The size of SkPath's external storage is not currently accounted for (and
    // may well be shared anyway).
    return 0;
  }
  int OpCount() const { return 1; }

  const SkPath clip_path;
  const bool antialias;
};

class CC_PAINT_EXPORT EndClipPathDisplayItem : public DisplayItem {
 public:
  EndClipPathDisplayItem();
  ~EndClipPathDisplayItem() override;

  int OpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PAINT_CLIP_PATH_DISPLAY_ITEM_H_

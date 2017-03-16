// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DRAWING_DISPLAY_ITEM_H_
#define CC_PLAYBACK_DRAWING_DISPLAY_ITEM_H_

#include <stddef.h>

#include "cc/base/cc_export.h"
#include "cc/paint/paint_record.h"
#include "cc/playback/display_item.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

class CC_EXPORT DrawingDisplayItem : public DisplayItem {
 public:
  DrawingDisplayItem();
  explicit DrawingDisplayItem(sk_sp<const PaintRecord> record);
  explicit DrawingDisplayItem(const DrawingDisplayItem& item);
  ~DrawingDisplayItem() override;

  size_t ExternalMemoryUsage() const;
  int ApproximateOpCount() const;

  const sk_sp<const PaintRecord> picture;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DRAWING_DISPLAY_ITEM_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_TRANSFORM_DISPLAY_ITEM_H_
#define CC_PLAYBACK_TRANSFORM_DISPLAY_ITEM_H_

#include <stddef.h>

#include "cc/base/cc_export.h"
#include "cc/playback/display_item.h"
#include "ui/gfx/transform.h"

namespace cc {

class CC_EXPORT TransformDisplayItem : public DisplayItem {
 public:
  explicit TransformDisplayItem(const gfx::Transform& transform);
  ~TransformDisplayItem() override;

  size_t ExternalMemoryUsage() const { return 0; }
  int ApproximateOpCount() const { return 1; }

  const gfx::Transform transform;
};

class CC_EXPORT EndTransformDisplayItem : public DisplayItem {
 public:
  EndTransformDisplayItem();
  ~EndTransformDisplayItem() override;

  int ApproximateOpCount() const { return 0; }
};

}  // namespace cc

#endif  // CC_PLAYBACK_TRANSFORM_DISPLAY_ITEM_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/transform_display_item.h"

namespace cc {

TransformDisplayItem::TransformDisplayItem(const gfx::Transform& transform)

    : DisplayItem(TRANSFORM), transform(transform) {}

TransformDisplayItem::~TransformDisplayItem() = default;

EndTransformDisplayItem::EndTransformDisplayItem()
    : DisplayItem(END_TRANSFORM) {}

EndTransformDisplayItem::~EndTransformDisplayItem() = default;

}  // namespace cc

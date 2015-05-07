// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/largest_display_item.h"

#include <algorithm>

#include "cc/resources/clip_display_item.h"
#include "cc/resources/clip_path_display_item.h"
#include "cc/resources/compositing_display_item.h"
#include "cc/resources/drawing_display_item.h"
#include "cc/resources/filter_display_item.h"
#include "cc/resources/float_clip_display_item.h"
#include "cc/resources/transform_display_item.h"

#include "third_party/skia/include/core/SkPicture.h"

namespace {
const size_t kLargestDisplayItemSize = sizeof(cc::TransformDisplayItem);
}  // namespace

namespace cc {

size_t LargestDisplayItemSize() {
  // Use compile assert to make sure largest is actually larger than all other
  // type of display_items.
  static_assert(sizeof(ClipDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. ClipDisplayItem"
                " is currently largest.");
  static_assert(sizeof(EndClipDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. EndClipDisplayItem"
                " is currently largest.");
  static_assert(sizeof(ClipPathDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. ClipPathDisplayItem"
                " is currently largest.");
  static_assert(sizeof(EndClipPathDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. EndClipPathDisplayItem"
                " is currently largest.");
  static_assert(sizeof(CompositingDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. CompositingDisplayItem"
                " is currently largest.");
  static_assert(sizeof(EndCompositingDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. EndCompositingDisplayItem"
                " is currently largest.");
  static_assert(sizeof(DrawingDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. DrawingDisplayItem"
                " is currently largest.");
  static_assert(sizeof(FilterDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. FilterDisplayItem"
                " is currently largest.");
  static_assert(sizeof(EndFilterDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. EndFilterDisplayItem"
                " is currently largest.");
  static_assert(sizeof(FloatClipDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. FloatClipDisplayItem"
                " is currently largest.");
  static_assert(sizeof(EndFloatClipDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. EndFloatClipDisplayItem"
                " is currently largest.");
  static_assert(sizeof(TransformDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. TransformDisplayItem"
                " is currently largest.");
  static_assert(sizeof(EndTransformDisplayItem) <= kLargestDisplayItemSize,
                "Largest Draw Quad size needs update. EndTransformDisplayItem"
                " is currently largest.");

  return kLargestDisplayItemSize;
}

}  // namespace cc

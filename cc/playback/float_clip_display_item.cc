// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/float_clip_display_item.h"

namespace cc {

FloatClipDisplayItem::FloatClipDisplayItem(const gfx::RectF& clip_rect)
    : DisplayItem(FLOAT_CLIP), clip_rect(clip_rect) {}

FloatClipDisplayItem::~FloatClipDisplayItem() = default;

EndFloatClipDisplayItem::EndFloatClipDisplayItem()
    : DisplayItem(END_FLOAT_CLIP) {}

EndFloatClipDisplayItem::~EndFloatClipDisplayItem() = default;

}  // namespace cc

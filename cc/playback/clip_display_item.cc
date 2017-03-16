// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/clip_display_item.h"

namespace cc {

ClipDisplayItem::ClipDisplayItem(const gfx::Rect& clip_rect,
                                 std::vector<SkRRect> rounded_clip_rects,
                                 bool antialias)
    : DisplayItem(CLIP),
      clip_rect(clip_rect),
      rounded_clip_rects(std::move(rounded_clip_rects)),
      antialias(antialias) {}

ClipDisplayItem::~ClipDisplayItem() = default;

EndClipDisplayItem::EndClipDisplayItem() : DisplayItem(END_CLIP) {}

EndClipDisplayItem::~EndClipDisplayItem() = default;

}  // namespace cc

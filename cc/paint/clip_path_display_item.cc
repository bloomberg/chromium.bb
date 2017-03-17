// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/clip_path_display_item.h"

namespace cc {

ClipPathDisplayItem::ClipPathDisplayItem(const SkPath& clip_path,
                                         bool antialias)
    : DisplayItem(CLIP_PATH), clip_path(clip_path), antialias(antialias) {}

ClipPathDisplayItem::~ClipPathDisplayItem() = default;

EndClipPathDisplayItem::EndClipPathDisplayItem() : DisplayItem(END_CLIP_PATH) {}

EndClipPathDisplayItem::~EndClipPathDisplayItem() = default;

}  // namespace cc

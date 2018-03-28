// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/render_frame_metadata.h"

namespace cc {

RenderFrameMetadata::RenderFrameMetadata() = default;

RenderFrameMetadata::RenderFrameMetadata(const RenderFrameMetadata& other) =
    default;

RenderFrameMetadata::RenderFrameMetadata(RenderFrameMetadata&& other) = default;

RenderFrameMetadata::~RenderFrameMetadata() {}

// static
bool RenderFrameMetadata::HasAlwaysUpdateMetadataChanged(
    const RenderFrameMetadata& rfm1,
    const RenderFrameMetadata& rfm2) {
  return rfm1.root_background_color != rfm2.root_background_color ||
         rfm1.is_scroll_offset_at_top != rfm2.is_scroll_offset_at_top;
}

RenderFrameMetadata& RenderFrameMetadata::operator=(
    const RenderFrameMetadata&) = default;

RenderFrameMetadata& RenderFrameMetadata::operator=(
    RenderFrameMetadata&& other) = default;

bool RenderFrameMetadata::operator==(const RenderFrameMetadata& other) {
  return root_scroll_offset == other.root_scroll_offset &&
         root_background_color == other.root_background_color &&
         is_scroll_offset_at_top == other.is_scroll_offset_at_top;
}

bool RenderFrameMetadata::operator!=(const RenderFrameMetadata& other) {
  return !operator==(other);
}

}  // namespace cc

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

bool RenderFrameMetadata::HasAlwaysUpdateMetadataChanged(
    const RenderFrameMetadata& rfm1,
    const RenderFrameMetadata& rfm2) {
  // TODO(jonross): as low frequency fields are added, update this method.
  return false;
}

RenderFrameMetadata& RenderFrameMetadata::operator=(
    const RenderFrameMetadata&) = default;

RenderFrameMetadata& RenderFrameMetadata::operator=(
    RenderFrameMetadata&& other) = default;

bool RenderFrameMetadata::operator==(const RenderFrameMetadata& other) {
  return root_scroll_offset == other.root_scroll_offset;
}

bool RenderFrameMetadata::operator!=(const RenderFrameMetadata& other) {
  return !operator==(other);
}

}  // namespace cc

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/image_processor.h"

namespace media {

ImageProcessor::PortConfig::PortConfig(
    const VideoFrameLayout& layout,
    const gfx::Size& visible_size,
    const std::vector<VideoFrame::StorageType>& preferred_storage_types)
    : layout(layout),
      visible_size(visible_size),
      preferred_storage_types(preferred_storage_types) {}

ImageProcessor::PortConfig::~PortConfig() {}

}  // namespace media

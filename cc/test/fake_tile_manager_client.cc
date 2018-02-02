// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager_client.h"

#include <vector>

namespace cc {

FakeTileManagerClient::FakeTileManagerClient() = default;

FakeTileManagerClient::~FakeTileManagerClient() = default;

std::unique_ptr<RasterTilePriorityQueue>
FakeTileManagerClient::BuildRasterQueue(TreePriority tree_priority,
                                        RasterTilePriorityQueue::Type type) {
  return nullptr;
}

std::unique_ptr<EvictionTilePriorityQueue>
FakeTileManagerClient::BuildEvictionQueue(TreePriority tree_priority) {
  return nullptr;
}

RasterColorSpace FakeTileManagerClient::GetRasterColorSpace() const {
  return RasterColorSpace();
}

size_t FakeTileManagerClient::GetFrameIndexForImage(
    const PaintImage& paint_image,
    WhichTree tree) const {
  return paint_image.frame_index();
}

}  // namespace cc

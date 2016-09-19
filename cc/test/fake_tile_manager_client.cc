// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager_client.h"

#include <vector>

namespace cc {

FakeTileManagerClient::FakeTileManagerClient() {
}

FakeTileManagerClient::~FakeTileManagerClient() {
}

std::unique_ptr<RasterTilePriorityQueue>
FakeTileManagerClient::BuildRasterQueue(TreePriority tree_priority,
                                        RasterTilePriorityQueue::Type type) {
  return nullptr;
}

std::unique_ptr<EvictionTilePriorityQueue>
FakeTileManagerClient::BuildEvictionQueue(TreePriority tree_priority) {
  return nullptr;
}

gfx::ColorSpace FakeTileManagerClient::GetTileColorSpace() const {
  return gfx::ColorSpace();
}

}  // namespace cc

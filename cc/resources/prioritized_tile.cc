// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_tile.h"

#include "cc/resources/picture_layer_tiling.h"

namespace cc {

PrioritizedTile::PrioritizedTile() : tile_(nullptr), is_occluded_(false) {
}

PrioritizedTile::PrioritizedTile(Tile* tile,
                                 const TilePriority priority,
                                 bool is_occluded)
    : tile_(tile), priority_(priority), is_occluded_(is_occluded) {
}

PrioritizedTile::~PrioritizedTile() {
}

}  // namespace cc

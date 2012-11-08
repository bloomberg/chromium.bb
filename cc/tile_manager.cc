// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_manager.h"

#include "base/logging.h"

namespace cc {

TileManager::TileManager(TileManagerClient* client) {
}

TileManager::~TileManager() {
  DCHECK(registered_tiles_.size() == 0);
}

void TileManager::RegisterTile(Tile* tile) {
  registered_tiles_.push_back(tile);
}

void TileManager::UnregisterTile(Tile* tile) {
  // TODO(nduca): Known slow. Shrug. Fish need frying.
  for (size_t i =  0; i < registered_tiles_.size(); i++) {
    if (registered_tiles_[i] == tile) {
      registered_tiles_.erase(registered_tiles_.begin() + i);
      return;
    }
  }
  DCHECK(false) << "Tile " << tile << " wasnt regitered.";
}

}

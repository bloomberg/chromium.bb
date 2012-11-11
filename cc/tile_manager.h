// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_MANAGER_H_
#define CC_TILE_MANAGER_H_

#include <vector>

#include "base/values.h"
#include "cc/tile_priority.h"

namespace cc {

class Tile;
class TileVersion;
class ResourceProvider;

class TileManagerClient {
 public:
  virtual void ScheduleManageTiles() = 0;

 protected:
  virtual ~TileManagerClient() {}
};

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class TileManager {
 public:
  TileManager(TileManagerClient* client);
  ~TileManager();

  void SetGlobalState(const GlobalStateThatImpactsTilePriority& state);
  void ManageTiles();

 protected:
  // Methods called by Tile
  void DidCreateTileVersion(TileVersion*);
  void WillModifyTileVersionPriority(TileVersion*, const TilePriority& new_priority);
  void DidDeleteTileVersion(TileVersion*);

 private:
  friend class Tile;
  void ScheduleManageTiles();

  TileManagerClient* client_;
  bool manage_tiles_pending_;

  GlobalStateThatImpactsTilePriority global_state_;
  std::vector<TileVersion*> tile_versions_;
};

}  // namespace cc

#endif  // CC_TILE_MANAGER_H_

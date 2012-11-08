// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_MANAGER_H_
#define CC_TILE_MANAGER_H_

#include <vector>

namespace cc {

class Tile;
class ResourceProvider;

class TileManagerClient {
 public:
  virtual void ScheduleManage() = 0;

 protected:
  ~TileManagerClient() { }
};

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class TileManager {
 public:
  TileManager(TileManagerClient* client);
  ~TileManager();
  void Manage() { }

 protected:
  // Methods called by Tile
  void RegisterTile(Tile*);
  void UnregisterTile(Tile*);

 private:
  friend class Tile;

  TileManagerClient* client_;
  std::vector<Tile*> registered_tiles_;
};

}

#endif

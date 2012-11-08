// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_H_
#define CC_TILE_H_

#include "base/memory/ref_counted.h"
#include "cc/picture_pile.h"
#include "cc/resource_provider.h"
#include "cc/tile_priority.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

class TileManager;

enum TileQuality {
  LOW_TILE_QUALITY,
  NORMAL_TILE_QUALITY
};

class Tile : public base::RefCounted<Tile> {
 public:
  Tile(TileManager* tile_manager,
       gfx::Size tile_size,
       gfx::Rect rect_inside_picture,
       TileQuality quality);

  void SetPicturePile(int frame_number, scoped_ptr<PicturePile> picture_pile) {}
  void SetPriority(int frame_number, TilePriority) {}

  bool IsDrawable(int frame_number) { return false; }
  ResourceProvider::ResourceId GetDrawableResourceId(int frame_number) { return 0; }

 private:
  friend class base::RefCounted<Tile>;
  friend class TileManager;

  ~Tile();

  TileManager* tile_manager_;
  gfx::Rect tile_size_;
  gfx::Rect rect_inside_picture_;
  TileQuality quality_;
};

}  // namespace cc
#endif

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_H_
#define CC_TILE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cc/picture_pile.h"
#include "cc/resource_provider.h"
#include "cc/tile_priority.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

class Tile;
class TileManager;

enum TileQuality {
  LOW_TILE_QUALITY,
  NORMAL_TILE_QUALITY
};

class TileVersion {
public:
  TileVersion(Tile* tile, int frame_number,
              PicturePile* picture_pile)
    : tile_(tile),
      frame_number_(frame_number),
      picture_pile_(picture_pile),
      resource_id_(0) {}

  int frame_number() const { return frame_number_; }

  const PicturePile* picture_pile() const {
    return picture_pile_;
  }

  const TilePriority& priority() const {
    return priority_;
  }

  void ModifyPriority(const TilePriority& priority) {
    priority_ = priority;
  }

  ResourceProvider::ResourceId resource_id() const {
    return resource_id_;
  }

private:
  Tile* tile_;
  int frame_number_;
  PicturePile* picture_pile_;
  TilePriority priority_;
  ResourceProvider::ResourceId resource_id_;
};

class Tile : public base::RefCounted<Tile> {
 public:
  Tile(TileManager* tile_manager,
       gfx::Size tile_size,
       GLenum format,
       gfx::Rect rect_inside_picture,
       TileQuality quality);

  void SetPicturePile(int frame_number, PicturePile* picture_pile);
  void ModifyPriority(int frame_number, const TilePriority& priority);

  // Returns 0 if not drawable.
  ResourceProvider::ResourceId GetDrawableResourceId(int frame_number);

  const gfx::Rect& opaque_rect() const { return opaque_rect_; }
  // TODO(enne): Make this real
  bool contents_swizzled() const { return false; }

 protected:
  // Methods called by TileManager.
  void DeleteVersionOnRequestOfTileManager(int frame_number);

 private:
  friend class base::RefCounted<Tile>;
  friend class TileManager;

  TileVersion* GetVersion(int frame_number);
  ~Tile();

  TileManager* tile_manager_;
  gfx::Rect tile_size_;
  GLenum format_;
  gfx::Rect rect_inside_picture_;
  gfx::Rect opaque_rect_;
  TileQuality quality_;
  ScopedVector<TileVersion> versions_;
};

}  // namespace cc

#endif  // CC_TILE_H_

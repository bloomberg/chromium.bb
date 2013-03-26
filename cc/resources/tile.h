// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_H_
#define CC_RESOURCES_TILE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cc/resources/managed_tile_state.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/tile_priority.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

class Tile;
class TileManager;

class CC_EXPORT Tile : public base::RefCounted<Tile> {
 public:
  Tile(TileManager* tile_manager,
       PicturePileImpl* picture_pile,
       gfx::Size tile_size,
       GLenum format,
       gfx::Rect content_rect,
       gfx::Rect opaque_rect,
       float contents_scale,
       int layer_id);

  PicturePileImpl* picture_pile() {
    return picture_pile_.get();
  }

  const TilePriority& priority(WhichTree tree) const {
    return priority_[tree];
  }

  TilePriority combined_priority() const {
    return TilePriority(priority_[ACTIVE_TREE],
                        priority_[PENDING_TREE]);
  }

  void SetPriority(WhichTree tree, const TilePriority& priority);

  scoped_ptr<base::Value> AsValue() const;

  const ManagedTileState::DrawingInfo& drawing_info() const {
    return managed_state_.drawing_info;
  }
  ManagedTileState::DrawingInfo& drawing_info() {
    return managed_state_.drawing_info;
  }

  const gfx::Rect& opaque_rect() const { return opaque_rect_; }

  float contents_scale() const { return contents_scale_; }
  gfx::Rect content_rect() const { return content_rect_; }

  int layer_id() const { return layer_id_; }

  void set_picture_pile(scoped_refptr<PicturePileImpl> pile) {
    DCHECK(pile->CanRaster(contents_scale_, content_rect_));
    picture_pile_ = pile;
  }

 private:
  // Methods called by by tile manager.
  friend class TileManager;
  friend class BinComparator;
  ManagedTileState& managed_state() { return managed_state_; }
  const ManagedTileState& managed_state() const { return managed_state_; }

  inline size_t bytes_consumed_if_allocated() const {
    DCHECK(format_ == GL_RGBA);
    return 4 * tile_size_.width() * tile_size_.height();
  }


  // Normal private methods.
  friend class base::RefCounted<Tile>;
  ~Tile();

  TileManager* tile_manager_;
  scoped_refptr<PicturePileImpl> picture_pile_;
  gfx::Rect tile_size_;
  GLenum format_;
  gfx::Rect content_rect_;
  float contents_scale_;
  gfx::Rect opaque_rect_;

  TilePriority priority_[NUM_BIN_PRIORITIES];
  ManagedTileState managed_state_;
  int layer_id_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_H_

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile.h"

#include "cc/tile_manager.h"

namespace cc {

Tile::Tile(TileManager* tile_manager,
           gfx::Size tile_size,
           GLenum format,
           gfx::Rect rect_inside_picture,
           TileQuality quality)
  : tile_manager_(tile_manager),
    tile_size_(tile_size),
    format_(format),
    rect_inside_picture_(rect_inside_picture),
    quality_(quality) {
}

Tile::~Tile() {
  for (size_t i = 0; i < versions_.size(); i++)
    tile_manager_->DidDeleteTileVersion(versions_[i]);
  versions_.clear();
}

void Tile::SetPicturePile(int frame_number, PicturePile* picture_pile) {
  DCHECK(GetVersion(frame_number) == NULL);
  TileVersion* version = new TileVersion(this, frame_number, picture_pile);
  versions_.push_back(version);
  tile_manager_->DidCreateTileVersion(version);
}

void Tile::ModifyPriority(int frame_number, const TilePriority& priority) {
  TileVersion* version = GetVersion(frame_number);
  DCHECK(version) <<
    "Tile must have a picture pile before its priority can be modified";
  tile_manager_->WillModifyTileVersionPriority(version, priority);
  version->ModifyPriority(priority);
}

ResourceProvider::ResourceId Tile::GetDrawableResourceId(int frame_number) {
  TileVersion* version = GetVersion(frame_number);
  if (!version)
    return 0;
  return version->resource_id();
}

void Tile::DeleteVersionOnRequestOfTileManager(int frame_number) {
  for (size_t i = 0; i < versions_.size(); i++) {
    if (versions_[i]->frame_number() == frame_number) {
      versions_.erase(versions_.begin() + frame_number);
      return;
    }
  }
  DCHECK(false) << "Could not find version";
}

TileVersion* Tile::GetVersion(int frame_number) {
  for (size_t i = 0; i < versions_.size(); i++) {
    if (versions_[i]->frame_number() == frame_number)
      return versions_[i];
  }
  return NULL;
}

}  // namespace cc

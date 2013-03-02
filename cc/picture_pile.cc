// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "cc/picture_pile.h"
#include "cc/picture_pile_impl.h"
#include "cc/region.h"

namespace {
// Maximum number of pictures that can overlap before we collapse them into
// a larger one.
const int kMaxOverlapping = 2;
// Maximum percentage area of the base picture another picture in the picture
// list can be.  If higher, we destroy the list and recreate from scratch.
const float kResetThreshold = 0.7f;
// Layout pixel buffer around the visible layer rect to record.  Any base
// picture that intersects the visible layer rect expanded by this distance
// will be recorded.
const int kPixelDistanceToRecord = 8000;
}

namespace cc {

PicturePile::PicturePile() {
}

PicturePile::~PicturePile() {
}

void PicturePile::Update(
    ContentLayerClient* painter,
    SkColor background_color,
    const Region& invalidation,
    gfx::Rect visible_layer_rect,
    RenderingStats* stats) {
  background_color_ = background_color;

  gfx::Rect interest_rect = visible_layer_rect;
  interest_rect.Inset(
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord,
      -kPixelDistanceToRecord);
  for (Region::Iterator i(invalidation); i.has_rect(); i.next()) {
    // Inflate all recordings from invalidations with a margin so that when
    // scaled down to at least min_contents_scale, any final pixel touched by an
    // invalidation can be fully rasterized by this picture.
    gfx::Rect inflated_invalidation = i.rect();
    inflated_invalidation.Inset(
        -buffer_pixels(),
        -buffer_pixels(),
        -buffer_pixels(),
        -buffer_pixels());
    // Split this inflated invalidation across tile boundaries and apply it
    // to all tiles that it touches.
    for (TilingData::Iterator iter(&tiling_, inflated_invalidation);
         iter; ++iter) {
      gfx::Rect tile =
          tiling_.TileBoundsWithBorder(iter.index_x(), iter.index_y());
      if (!tile.Intersects(interest_rect)) {
        // This invalidation touches a tile outside the interest rect, so
        // just remove the entire picture list.
        picture_list_map_.erase(iter.index());
        continue;
      }

      gfx::Rect tile_invalidation =
          gfx::IntersectRects(inflated_invalidation, tile);
      if (tile_invalidation.IsEmpty())
        continue;
      PictureListMap::iterator find = picture_list_map_.find(iter.index());
      if (find == picture_list_map_.end())
        continue;
      PictureList& pic_list = find->second;
      // Leave empty pic_lists empty in case there are multiple invalidations.
      if (!pic_list.empty())
        InvalidateRect(pic_list, tile_invalidation);
    }
  }

  // Walk through all pictures in the rect of interest and record.
  for (TilingData::Iterator iter(&tiling_, interest_rect); iter; ++iter) {
    // Create a picture in this list if it doesn't exist.
    PictureList& pic_list = picture_list_map_[iter.index()];
    if (pic_list.empty()) {
      // Inflate the base picture with a margin, similar to invalidations, so
      // that when scaled down to at least min_contents_scale, the enclosed
      // rect still includes content all the way to the edge of the layer.
      gfx::Rect tile = tiling_.TileBounds(iter.index_x(), iter.index_y());
      tile.Inset(
        -buffer_pixels(),
        -buffer_pixels(),
        -buffer_pixels(),
        -buffer_pixels());
      scoped_refptr<Picture> base_picture = Picture::Create(tile);
      pic_list.push_back(base_picture);
    }

    for (PictureList::iterator pic = pic_list.begin();
         pic != pic_list.end(); ++pic) {
      if (!(*pic)->HasRecording())
        (*pic)->Record(painter, stats, tile_grid_info_);
    }
  }

  UpdateRecordedRegion();
}

class FullyContainedPredicate {
public:
  FullyContainedPredicate(gfx::Rect rect) : layer_rect_(rect) { }
  bool operator()(const scoped_refptr<Picture>& picture) {
    return layer_rect_.Contains(picture->LayerRect());
  }
  gfx::Rect layer_rect_;
};

void PicturePile::InvalidateRect(
    PictureList& picture_list,
    gfx::Rect invalidation) {
  DCHECK(!picture_list.empty());

  std::vector<PictureList::iterator> overlaps;
  for (PictureList::iterator i = picture_list.begin();
       i != picture_list.end(); ++i) {
    if ((*i)->LayerRect().Contains(invalidation) && !(*i)->HasRecording())
      return;
    if ((*i)->LayerRect().Intersects(invalidation) && i != picture_list.begin())
      overlaps.push_back(i);
  }

  gfx::Rect picture_rect = invalidation;
  if (overlaps.size() >= kMaxOverlapping) {
    for (size_t j = 0; j < overlaps.size(); j++)
      picture_rect.Union((*overlaps[j])->LayerRect());
  }

  Picture* base_picture = picture_list.front();
  int max_pixels = kResetThreshold * base_picture->LayerRect().size().GetArea();
  if (picture_rect.size().GetArea() > max_pixels) {
    // This picture list will be entirely recreated, so clear it.
    picture_list.clear();
    return;
  }

  FullyContainedPredicate pred(picture_rect);
  picture_list.erase(std::remove_if(picture_list.begin(),
                                    picture_list.end(),
                                    pred),
                     picture_list.end());
  picture_list.push_back(Picture::Create(picture_rect));
}


void PicturePile::PushPropertiesTo(PicturePileImpl* other) {
  // TODO(enne): Don't clear clones or push anything if nothing has changed
  // on this layer this frame.
  PicturePileBase::PushPropertiesTo(other);
  other->clones_.clear();
}

}  // namespace cc
